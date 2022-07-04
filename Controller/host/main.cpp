#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdbool.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <math.h>
#include <sys/mman.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <sstream>
#include <unistd.h>
#include "hwlib.h"
#include "socal/socal.h"
#include "socal/hps.h"
#include "socal/alt_gpio.h"
#include "socal/alt_uart.h"
#include "hps_0.h" 
#include "uart_core_lib.h"
#include "Command.h"
#include "Queue.h"
#include "QueueCommand.h"
#include "terasic_os.h"
#include "CL/opencl.h"
#include "aocl_utils.h"
#include "gmnet_params.h"

#define HW_REGS_BASE ( ALT_STM_OFST )
#define HW_REGS_SPAN ( 0x04000000 )
#define HW_REGS_MASK ( HW_REGS_SPAN - 1 )
#define DEBUG_DUMP  /*printf */

using namespace std;
using namespace aocl_utils;

enum {FALSE, TRUE};

typedef alt_u8 	uint8_t;
typedef alt_8 	int8_t;
typedef alt_u16 uint16_t;
typedef alt_16 	int16_t;
typedef alt_u32 uint32_t;
typedef alt_32 	int32_t;

typedef enum{
	CMD_MOI,
	CMD_TMP,
	CMD_HUM,
	CMD_LIT,
	CMD_CO2,
	CMD_CAM,
	CMD_LED_ON,
	CMD_LED_OFF,
	CMD_WTR_ON,
	CMD_WTR_OFF,
	CMD_FAN_ON,
	CMD_FAN_OFF,
	CMD_IDLE,
}COMMAND_ID;

typedef struct controller_input {
    float co2;
    float temp;
    float light;
    float humidity;
    float soilmos;
    int imCat; // Image category
} conin;

typedef struct controller_output {
    bool fan;
    int water;
    int light; // 0 = off, 1 = blue, 2 = red
    bool warning;
} conout;

// OpenCL runtime configuration
cl_platform_id platform = NULL;
unsigned num_devices = 0;
scoped_array<cl_device_id> device; // num_devices elements
cl_context context = NULL;
cl_command_queue queue;
cl_program program = NULL;

//host buffer
float *h_result_classifier = (float *)malloc((2) * sizeof(float)); 
char class_label[200];

cl_kernel conv3x3_1, conv3x3_2, conv3x3_3, maxpool_1, maxpool_2, maxpool_3, dense_1, dense_2;

cl_mem d_sample, d_conv1_weight, d_conv1_bias, d_result_conv1, d_result_pool1;
cl_mem d_conv2_weight, d_conv2_bias, d_result_conv2, d_result_pool2;
cl_mem d_conv3_weight, d_conv3_bias, d_result_conv3, d_result_pool3;
cl_mem d_dense1_weight, d_dense1_bias, d_result_dense1;
cl_mem d_dense2_weight, d_dense2_bias, d_result_classifier;

void getLabel(unsigned int class_index);
void cleanup();

void createDevice(conin dd, conout dc) {
    dd.co2 = 0; dd.temp = 0; dd.light = 0; dd.humidity = 0; dd.soilmos = 0; dd.imCat = 0;
    dc.fan = false; dc.water = 0; dc.light = 0; dc.warning = false;
}

conout controller(conin dd){
    conout dc;
    switch (dd.imCat) {
        case 1: // Plant needs water
            if (dd.soilmos < 70) dc.water = (90 - dd.soilmos);
            break;

        case 2: // Plant is flowering
            dc.light = 2;
            dc.warning = true;
            break;

        case 3: case 4: // Plant is dead
            dc.warning = true;
            break;

        case 5: // Plant is dark
            if (dd.light < 2.75) dc.light = 1;
            break;

        default: // Case 0 plant is healthy
            if ((dd.co2>10000|dd.co2<300) & dd.temp>36 & dd.light<2.75 & (dd.humidity>95&dd.humidity<70) & dd.soilmos<40) dc.warning = true;
            else {

                if (dd.co2>10000|dd.co2<300) dc.fan = true;
                else dc.fan = false;

                if (dd.temp>32) {dc.fan = true; dc.water = 100;}
                else {dc.fan = false; dc.water = 0;}

                if (dd.light<2.75) dc.light = 1;
                else dc.light = 0;

                if (dd.humidity<75) {dc.water = 100; dc.fan = true;}
                else {dc.fan = false; dc.water = 0;}

                if (dd.soilmos<50) dc.water = (90 - dd.soilmos);
                else dc.water = 0;
            }
            break;
    }
    return dc;
}

int main()
{
	printf("1\n"); // Debug milestones

	ofstream sensor_data;

	int fd;
	void *virtual_base;
	void *uart_base;

	printf("2\n"); // Debug milestones

	printf("ALT_STM_OFST Value: 0x%08x\n", ALT_STM_OFST);
	printf("ALT_LWFPGASLVS_OFST Value: 0x%08x\n", ALT_LWFPGASLVS_OFST);

	// map the address space for the LED registers into user space so we can interact with them.
	// we'll actually map in the entire CSR span of the HPS since we want to access various registers within that span
	if ((fd = open( "/dev/mem", (O_RDWR | O_SYNC))) == -1) 
	{
		printf("ERROR: could not open \"/dev/mem\"...\n");
		return -1;
	}

	printf("3\n"); // Debug milestones

	virtual_base = mmap( NULL, HW_REGS_SPAN, ( PROT_READ | PROT_WRITE ), MAP_SHARED, fd, HW_REGS_BASE );

	if (virtual_base == MAP_FAILED) 
	{
		printf("ERROR: mmap() failed...\n");
		close(fd);
		return -1;
	}
	
	printf("4\n"); // Debug milestones

	// OpenCL Configuration
	cl_int status;

	printf("Initializing OpenCL\n");

	if(!setCwdToExeDir()) {
		return 1;
	}

	// Get the OpenCL platform.
	platform = findPlatform("Intel(R) FPGA");
	if(platform == NULL) {
		printf("ERROR: Unable to find Intel(R) FPGA OpenCL platform.\n");
		return 1;
	}

	// Query the available OpenCL device.
	device.reset(getDevices(platform, CL_DEVICE_TYPE_ALL, &num_devices));
	printf("Platform: %s\n", getPlatformName(platform).c_str());
	printf("Using %d device(s)\n", num_devices);
	for(unsigned int i = 0; i < num_devices; ++i) {
		printf("  %s\n", getDeviceName(device[i]).c_str());
	}

	// Create the context.
	context = clCreateContext(NULL, num_devices, device, NULL, NULL, &status);
	checkError(status, "Failed to create context");

	// Create the program for all device. Use the first device as the
	// representative device (assuming all device are of the same type).
	std::string binary_file = getBoardBinaryFile("green_machine", device[0]);
	printf("Using AOCX: %s\n", binary_file.c_str());
	program = createProgramFromBinary(context, binary_file.c_str(), device, num_devices);

	// Build the program that was just created.
	status = clBuildProgram(program, 0, NULL, "", NULL, NULL);
	checkError(status, "Failed to build program");

	queue = clCreateCommandQueue(context, device[0], CL_QUEUE_PROFILING_ENABLE, &status);
	checkError(status, "Failed to create command queue");

	// Kernel.
	const char *kernel1 = "conv2d3x3";
	conv3x3_1 = clCreateKernel(program, kernel1, &status);
	checkError(status, "Failed to create kernel conv2d11x11");

	const char *kernel2 = "maxpool2d";
	maxpool_1 = clCreateKernel(program, kernel2, &status);
	checkError(status, "Failed to create kernel maxpool2d");

	const char *kernel3 = "conv2d3x3";
	conv3x3_2 = clCreateKernel(program, kernel3, &status);
	checkError(status, "Failed to create kernel conv2d9x9");

	const char *kernel4 = "maxpool2d";
	maxpool_2 = clCreateKernel(program, kernel4, &status);
	checkError(status, "Failed to create kernel maxpool2d");

	const char *kernel5 = "conv2d3x3";
	conv3x3_3 = clCreateKernel(program, kernel5, &status);
	checkError(status, "Failed to create kernel conv2d5x5");

	const char *kernel6 = "maxpool2d";
	maxpool_3 = clCreateKernel(program, kernel6, &status);
	checkError(status, "Failed to create kernel maxpool2d");

	const char *kernel7 = "dense";
	dense_1 = clCreateKernel(program, kernel7, &status);
	checkError(status, "Failed to create kernel dense1");

	const char *kernel8 = "dense";
	dense_2 = clCreateKernel(program, kernel8, &status);
	checkError(status, "Failed to create kernel dense2");

	// Define addresses for UART
	uart_base = virtual_base + ( ( unsigned long  )( ALT_LWFPGASLVS_OFST + HC05_UART_BASE) & ( unsigned long)( HW_REGS_MASK ) );

	CQueueCommand QueueCommand;
    int Command, Param;
    bool bSleep = false;
    pthread_t id0;
    int ret0;

	// Initializing the Bluetooth UART 
	UART_T uart = uart_init(uart_base);
	setBaud(&uart, HC05_UART_FREQ, HC05_UART_BAUD);
	freeBuffer(&uart);
	checkUartRegisters(&uart);
	resetStatus(&uart);

	conin dev1data;
	conout dev1sig;
	int *image;
	createDevice(dev1data, dev1sig);

	// Enabling automatic reception using threads
	activateRecepcion(&uart, (void *)&QueueCommand);

	// Write the iterative function
	while (1) 
	{	
		if(!QueueCommand.IsEmpty() && QueueCommand.Pop(&Command, &Param) ){
			switch(Command){
			case CMD_MOI:printf("CMD_MOI\n");
						dev1data.soilmos = Param;		
						break;
			case CMD_TMP:printf("CMD_TMP\n");
						dev1data.temp = Param;
						break;

			case CMD_HUM:printf("CMD_HUM\n");
						dev1data.humidity = Param;
						break;

			case CMD_LIT:printf("CMD_LIT\n");
						dev1data.light = Param;
						break;
			
			case CMD_CO2:printf("CMD_CO2\n");
						dev1data.co2 = Param;
						break;

			case CMD_CAM:
						int i = 0;
						while (!QueueCommand.IsEmpty() && QueueCommand.Pop(&Command, &Param) && Command == CMD_CAM){
							image[i] = Param;
							i++;
						}

						/**************************************************************/
						/*                          conv1                             */
						/**************************************************************/
						printf("\r\nCNN on FPGA start:\r\n");
						double total = 0.0;
						double start_time = getCurrentTimestamp();
						// printf("1\n");

						unsigned int input_channel, input_h, input_w, pad_1, stride_1, start_channel, output_h, output_w;
						
						input_channel = 3;
						input_h = 480;
						input_w = 640;
						pad_1 = 0; 
						stride_1 = 4; 
						start_channel = 0;
						output_h = 120;
						output_w = 160;
						// printf("2\n");

						status |= clSetKernelArg(conv3x3_1, 0, sizeof(int), &(input_channel));
						// printf("3\n");
						status |= clSetKernelArg(conv3x3_1, 1, sizeof(int), &(input_h));
						// printf("4\n");
						status |= clSetKernelArg(conv3x3_1, 2, sizeof(int), &(input_w));
						// printf("5\n");
						status |= clSetKernelArg(conv3x3_1, 3, sizeof(int), &(pad_1));
						// printf("6\n");
						status |= clSetKernelArg(conv3x3_1, 4, sizeof(int), &(stride_1));
						// printf("7\n");
						status |= clSetKernelArg(conv3x3_1, 5, sizeof(int), &(start_channel));
						// printf("8\n");
						status |= clSetKernelArg(conv3x3_1, 6, sizeof(int), &(output_h));
						// printf("9\n");
						status |= clSetKernelArg(conv3x3_1, 7, sizeof(int), &(output_w));
						// printf("10\n");
						status |= clSetKernelArg(conv3x3_1, 8, sizeof(cl_mem), &d_sample);
						// printf("11\n");
						status |= clSetKernelArg(conv3x3_1, 9, sizeof(cl_mem), &d_conv1_weight);
						// printf("12\n");
						status |= clSetKernelArg(conv3x3_1, 10, sizeof(cl_mem), &d_conv1_bias);
						// printf("13\n");
						status |= clSetKernelArg(conv3x3_1, 11, sizeof(cl_mem), &d_result_conv1);
						checkError(status, "Setting conv1: conv3x3_1 arguments");
						// printf("14\n");

						size_t global_f[2] = {2,120}; 
						status = clEnqueueNDRangeKernel(queue, conv3x3_1, 2, NULL, global_f, NULL, 0, NULL, NULL);
						checkError(status, "Enqueueing conv1: conv3x3_1");

						input_h = 120;
						input_w = 160;
						output_h = 60;
						output_w = 80;
						status |= clSetKernelArg(maxpool_1, 0, sizeof(int), &(input_h));
						status |= clSetKernelArg(maxpool_1, 1, sizeof(int), &(input_w));
						status |= clSetKernelArg(maxpool_1, 2, sizeof(int), &(output_h));
						status |= clSetKernelArg(maxpool_1, 3, sizeof(int), &(output_w));
						status |= clSetKernelArg(maxpool_1, 4, sizeof(cl_mem), &d_result_conv1);
						status |= clSetKernelArg(maxpool_1, 5, sizeof(cl_mem), &d_result_pool1);
						checkError(status, "Setting maxpool_1 arguments");

						size_t global = 2;
						status = clEnqueueNDRangeKernel(queue, maxpool_1, 1, NULL, &global, NULL, 0, NULL, NULL);
						checkError(status, "Enqueueing maxpool_1");

						status = clFinish(queue);
						checkError(status, "Wait for maxpool_1 finish");

						double end_time = getCurrentTimestamp();
						printf("\r\nconv1 takes: %0.3f ms\r\n", (end_time - start_time) * 1e3);
						total += (end_time - start_time);
						start_time = end_time;
						
						/**************************************************************/
						/*                          conv2                             */
						/**************************************************************/
						total = 0.0;
						start_time = getCurrentTimestamp();

						unsigned int pad_2, stride_2;
						
						input_channel = 2;
						input_h = 60;
						input_w = 80;
						pad_2 = 0; 
						stride_2 = 2; 
						start_channel = 0;
						output_h = 29;
						output_w = 39;

						status |= clSetKernelArg(conv3x3_2, 0, sizeof(int), &(input_channel));
						status |= clSetKernelArg(conv3x3_2, 1, sizeof(int), &(input_h));
						status |= clSetKernelArg(conv3x3_2, 2, sizeof(int), &(input_w));
						status |= clSetKernelArg(conv3x3_2, 3, sizeof(int), &(pad_2));
						status |= clSetKernelArg(conv3x3_2, 4, sizeof(int), &(stride_2));
						status |= clSetKernelArg(conv3x3_2, 5, sizeof(int), &(start_channel));
						status |= clSetKernelArg(conv3x3_2, 6, sizeof(int), &(output_h));
						status |= clSetKernelArg(conv3x3_2, 7, sizeof(int), &(output_w));
						status |= clSetKernelArg(conv3x3_2, 8, sizeof(cl_mem), &d_result_pool1);
						status |= clSetKernelArg(conv3x3_2, 9, sizeof(cl_mem), &d_conv2_weight);
						status |= clSetKernelArg(conv3x3_2, 10, sizeof(cl_mem), &d_conv2_bias);
						status |= clSetKernelArg(conv3x3_2, 11, sizeof(cl_mem), &d_result_conv2);
						checkError(status, "Setting conv2: conv3x3_2 arguments");

						global_f[0] = 4;
						global_f[1] = 29;
						status = clEnqueueNDRangeKernel(queue, conv3x3_2, 2, NULL, global_f, NULL, 0, NULL, NULL);
						checkError(status, "Enqueueing conv2: conv3x3_2");

						input_h = 29;
						input_w = 39;
						output_h = 14;
						output_w = 19;
						status |= clSetKernelArg(maxpool_2, 0, sizeof(int), &(input_h));
						status |= clSetKernelArg(maxpool_2, 1, sizeof(int), &(input_h));
						status |= clSetKernelArg(maxpool_2, 2, sizeof(int), &(output_h));
						status |= clSetKernelArg(maxpool_2, 3, sizeof(int), &(output_w));
						status |= clSetKernelArg(maxpool_2, 4, sizeof(cl_mem), &d_result_conv2);
						status |= clSetKernelArg(maxpool_2, 5, sizeof(cl_mem), &d_result_pool2);
						checkError(status, "Setting maxpool_2 arguments");

						global = 4;
						status = clEnqueueNDRangeKernel(queue, maxpool_2, 1, NULL, &global, NULL, 0, NULL, NULL);
						checkError(status, "Enqueueing maxpool_2");

						status = clFinish(queue);
						checkError(status, "Wait for maxpool_2 finish");

						end_time = getCurrentTimestamp();
						printf("\r\nconv2 takes: %0.3f ms\r\n", (end_time - start_time) * 1e3);
						total += (end_time - start_time);
						start_time = end_time;
						
						/**************************************************************/
						/*                          conv3                             */
						/**************************************************************/
						start_time = getCurrentTimestamp();

						unsigned int pad_3, stride_3;
						
						input_channel = 4;
						input_h = 14;
						input_w = 19;
						pad_3 = 0; //////////////////////////////////////////////////////////////////////////////
						stride_3 = 2; ///////////////////////////////////////////////////////////////////////////
						start_channel = 0;
						output_h = 6;
						output_w = 9;

						status |= clSetKernelArg(conv3x3_3, 0, sizeof(int), &(input_channel));
						status |= clSetKernelArg(conv3x3_3, 1, sizeof(int), &(input_h));
						status |= clSetKernelArg(conv3x3_3, 2, sizeof(int), &(input_w));
						status |= clSetKernelArg(conv3x3_3, 3, sizeof(int), &(pad_3));
						status |= clSetKernelArg(conv3x3_3, 4, sizeof(int), &(stride_3));
						status |= clSetKernelArg(conv3x3_3, 5, sizeof(int), &(start_channel));
						status |= clSetKernelArg(conv3x3_3, 6, sizeof(int), &(output_h));
						status |= clSetKernelArg(conv3x3_3, 7, sizeof(int), &(output_w));
						status |= clSetKernelArg(conv3x3_3, 8, sizeof(cl_mem), &d_result_pool2);
						status |= clSetKernelArg(conv3x3_3, 9, sizeof(cl_mem), &d_conv3_weight);
						status |= clSetKernelArg(conv3x3_3, 10, sizeof(cl_mem), &d_conv3_bias);
						status |= clSetKernelArg(conv3x3_3, 11, sizeof(cl_mem), &d_result_conv3);
						checkError(status, "Setting conv3: conv3x3_3 arguments");

						
						global_f[0] = 4;
						global_f[1] = 6; //////////////////////////////////////////////////////////////////////////
						status = clEnqueueNDRangeKernel(queue, conv3x3_3, 2, NULL, global_f, NULL, 0, NULL, NULL);
						checkError(status, "Enqueueing conv3: conv3x3_3");

						input_h = 6;
						input_w = 9;
						output_h = 3;
						output_w = 4;
						status |= clSetKernelArg(maxpool_3, 0, sizeof(int), &(input_h));
						status |= clSetKernelArg(maxpool_3, 1, sizeof(int), &(input_w));
						status |= clSetKernelArg(maxpool_3, 2, sizeof(int), &(output_h));
						status |= clSetKernelArg(maxpool_3, 3, sizeof(int), &(output_w));
						status |= clSetKernelArg(maxpool_3, 4, sizeof(cl_mem), &d_result_conv3);
						status |= clSetKernelArg(maxpool_3, 5, sizeof(cl_mem), &d_result_pool3);
						checkError(status, "Setting maxpool_3 arguments");

						global = 4;
						status = clEnqueueNDRangeKernel(queue, maxpool_3, 1, NULL, &global, NULL, 0, NULL, NULL);
						checkError(status, "Enqueueing maxpool_3");

						status = clFinish(queue);
						checkError(status, "Wait for maxpool_3 finish");

						end_time = getCurrentTimestamp();
						printf("\r\nconv3 takes: %0.3f ms\r\n", (end_time - start_time) * 1e3);
						total += (end_time - start_time);
						start_time = end_time;
						
						/**************************************************************/
						/*                       classifier                           */
						/**************************************************************/
						unsigned int input_size, output_size;
						input_size = 48;
						output_size = 64;

						status |= clSetKernelArg(dense_1, 0, sizeof(int), &(input_size));
						status |= clSetKernelArg(dense_1, 1, sizeof(int), &(input_size));
						status |= clSetKernelArg(dense_1, 2, sizeof(cl_mem), &d_result_pool3);
						status |= clSetKernelArg(dense_1, 3, sizeof(cl_mem), &d_dense1_weight);
						status |= clSetKernelArg(dense_1, 4, sizeof(cl_mem), &d_dense1_bias);
						status |= clSetKernelArg(dense_1, 5, sizeof(cl_mem), &d_result_dense1);
						checkError(status, "Setting dense_1 arguments");

						size_t global_fd[] = {48};
						status = clEnqueueNDRangeKernel(queue, dense_1, 2, NULL, global_fd, NULL, 0, NULL, NULL);
						checkError(status, "Enqueueing dense_1");

						input_size = 64;
						output_size = 6;

						status |= clSetKernelArg(dense_2, 0, sizeof(int), &(input_size));
						status |= clSetKernelArg(dense_2, 1, sizeof(int), &(input_size));
						status |= clSetKernelArg(dense_2, 2, sizeof(cl_mem), &d_result_dense1);
						status |= clSetKernelArg(dense_2, 3, sizeof(cl_mem), &d_dense2_weight);
						status |= clSetKernelArg(dense_2, 4, sizeof(cl_mem), &d_dense2_bias);
						status |= clSetKernelArg(dense_2, 5, sizeof(cl_mem), &d_result_classifier);
						checkError(status, "Setting dense_2 arguments");

						global_fd[0] = 2;
						status = clEnqueueNDRangeKernel(queue, dense_2, 2, NULL, global_fd, NULL, 0, NULL, NULL);
						checkError(status, "Enqueueing dense_2");

						status = clFinish(queue);
						checkError(status, "Wait for classifier finish");
						end_time = getCurrentTimestamp();
						status = clEnqueueReadBuffer(queue, d_result_classifier, CL_TRUE, 0, sizeof(float) * 2, h_result_classifier, 0, NULL, NULL );

						float tmp = 0.0f;
						unsigned int class_index = 0;
						for(int j = 0; j < 2; j++)
						{
							if(h_result_classifier[j] > tmp)
							{
							tmp = h_result_classifier[j];
							class_index = j;
							}
						}
						printf("classifier takes: %0.3f ms\r\n", (end_time - start_time) * 1e3);
						total += (end_time - start_time);
						printf("total: %0.3f ms\r\n", total * 1e3);
						getLabel(class_index);
						printf("\r\npredicted label: %s\r\n", class_label);
						cleanup();

						dev1data.imCat = class_index;

						break;
			}

			dev1sig = controller(dev1data);

			if (dev1sig.fan) QueueCommand.Push(CMD_FAN_ON);
			else QueueCommand.Push(CMD_FAN_OFF);

			if (dev1sig.light) QueueCommand.Push(CMD_LED_ON);
			else QueueCommand.Push(CMD_LED_OFF);

			if (dev1sig.water) QueueCommand.Push(CMD_WTR_ON);
			else QueueCommand.Push(CMD_WTR_OFF);

			sensor_data.open("sensor_data.csv");
			sensor_data << dev1data.co2 << "," << dev1data.humidity << "," << dev1data.light << "," << dev1data.soilmos << "," << dev1data.temp << "," << dev1data.imCat << ",";
			char result[3];
			int j = 0;
			if (dev1sig.fan){
				result[j] = 'F';
				j++;
			}
			if (dev1sig.light){
				result[j] = 'L';
				j++;
			}
			if (dev1sig.water){
				result[j] = 'W';
				j++;
			}
			if (j == 0) result[j] = 'N';
			sensor_data << result << "\n";
		}	
	}	

	terminateRecepcion(&uart);
	int num;
	emptyBuffer(&uart, &num)
	// Clean up our memory mapping and exit
	if( munmap( virtual_base, HW_REGS_SPAN ) != 0 ) {
		printf( "ERROR: munmap() failed...\n" );
		close( fd );
		return( 1 );
	}


	close(fd);
	return 0;
}

void getLabel(unsigned int class_index)
{
  int i;
  
  FILE *fp;

  fp = fopen("labeled_photos", "r");
  for(i = 0; i < class_index + 1; i++)
  {
    fgets(class_label, sizeof(class_label), fp);
  }
  fclose(fp);
}

void cleanup()
{
  clReleaseMemObject(d_sample);
  clReleaseMemObject(d_conv1_weight);
  clReleaseMemObject(d_conv1_bias);
  clReleaseMemObject(d_result_conv1);
  clReleaseMemObject(d_result_pool1);

  clReleaseMemObject(d_conv2_weight);
  clReleaseMemObject(d_conv2_bias);
  clReleaseMemObject(d_result_conv2);
  clReleaseMemObject(d_result_pool2);

  clReleaseMemObject(d_conv3_weight);
  clReleaseMemObject(d_conv3_bias);
  clReleaseMemObject(d_result_conv3);
  clReleaseMemObject(d_result_pool3);

  clReleaseMemObject(d_dense1_weight);
  clReleaseMemObject(d_dense1_bias);
  clReleaseMemObject(d_result_dense1);

  clReleaseMemObject(d_dense2_weight);
  clReleaseMemObject(d_dense2_bias);
  clReleaseMemObject(d_result_classifier);

  clReleaseKernel(conv3x3_1);
  clReleaseKernel(conv3x3_2);
  clReleaseKernel(conv3x3_3);
  clReleaseKernel(maxpool_1);
  clReleaseKernel(maxpool_2);
  clReleaseKernel(maxpool_3);
  clReleaseKernel(dense_1);
  clReleaseKernel(dense_2);
	
  clReleaseCommandQueue(queue);
  clReleaseCommandQueue(queue);
  clReleaseProgram(program);
  clReleaseContext(context);

  free(h_result_classifier);
}
