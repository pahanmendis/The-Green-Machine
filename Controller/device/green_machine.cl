//maxPool2d 
//kernel_size=2 stride=1
//output one feature map per kernel
__kernel void maxpool2d(
	const int input_h,
	const int input_w,
	const int output_h,
	const int output_w,
	__global float *input_im,
    __global float *restrict output_im)
{
	int channels = get_global_id(0);	//get output channel index
	
	input_im += channels * input_h * input_w;
	output_im += channels * output_h * output_w;

	//loop over output feature map
	for(int i = 0; i < output_h; i++)//row
	{
		for(int j = 0; j < output_w; j++)//col
		{
			//find the max value in 2x2 reigon 
			//to be one element in the output feature map
			float tmp = 0.0;

			#pragma unroll 1
			for(int k = 0; k < 2; k++)//row
			{
				#pragma unroll 1
				for(int l = 0; l < 2; l++)//col
				{
					float value = input_im[(i * 2 + k) * input_w  + j * 2 + l ];
					if(value > tmp)
						tmp = value;
				}
			}
			//store the result to output feature map
			output_im[i * output_w + j] = tmp; 
		}
	}
}

// //11x11 convolution layer
// //output one feature map per kernel
// __kernel void conv2d11x11(
// 	const int input_channels, const int input_h, const int input_w,
// 	const int pad, const int stride,
// 	const int start_channel, 
// 	const int output_h, const int output_w,
// 	__global float* input_im,
// 	__global const float* filter_weight,
// 	__global const float* filter_bias,
// 	__global float *restrict output_im
// 	)
// {
// 	int filter_index = get_global_id(0); //get output channel index
// 	int i =  get_global_id(1);

// 	filter_weight += filter_index * input_channels * 121;
// 	float bias = filter_bias[filter_index];
// 	output_im += (start_channel + filter_index) * output_h * output_w;
	
// 	//loop over output feature map
// 	// for(int i = 0; i < output_h; i++)
// 	{
// 		for(int j = 0; j < output_w; j++)
// 		{
// 			//compute one element in the output feature map
// 			float tmp = bias;
			
// 			//compute dot product of 2 input_channels x 11 x 11 matrix
// 			for(int k = 0; k < input_channels; k++)
// 			{
// 				#pragma unroll
// 				for(int l = 0; l < 11; l++)
// 				{
// 					int h = i * stride + l - pad;
// 					for(int m = 0; m < 11; m++)
// 					{
// 						int w = j * stride + m - pad;
// 						if((h >= 0) && (h < input_h) && (w >= 0) && (w < input_w))
// 						{
// 							tmp += input_im[k * input_h * input_w + h * input_w + w] \
//                                * filter_weight[121 * k + 11 * l + m];
// 						}
// 					}
// 				}
// 			}

// 			//add relu activation after conv
// 			output_im[i * output_w + j] = (tmp > 0.0) ? tmp : 0.0;
// 		}
// 	}
// }

// //9x9 convolution layer
// //output one feature map per kernel
// __kernel void conv2d9x9(
// 	const int input_channels, const int input_h, const int input_w,
// 	const int pad, const int stride,
// 	const int start_channel, 
// 	const int output_h, const int output_w,
// 	__global float* input_im,
// 	__global const float* filter_weight,
// 	__global const float* filter_bias,
// 	__global float *restrict output_im
// 	)
// {
// 	int filter_index = get_global_id(0); //get output channel index
// 	int i =  get_global_id(1);

// 	filter_weight += filter_index * input_channels * 81;
// 	float bias = filter_bias[filter_index];
// 	output_im += (start_channel + filter_index) * output_h * output_w;
	
// 	//loop over output feature map
// 	// for(int i = 0; i < output_h; i++)
// 	{
// 		for(int j = 0; j < output_w; j++)
// 		{
// 			//compute one element in the output feature map
// 			float tmp = bias;
			
// 			//compute dot product of 2 input_channels x 9 x 9 matrix
// 			for(int k = 0; k < input_channels; k++)
// 			{
// 				#pragma unroll
// 				for(int l = 0; l < 9; l++)
// 				{
// 					int h = i * stride + l - pad;
// 					for(int m = 0; m < 9; m++)
// 					{
// 						int w = j * stride + m - pad;
// 						if((h >= 0) && (h < input_h) && (w >= 0) && (w < input_w))
// 						{
// 							tmp += input_im[k * input_h * input_w + h * input_w + w] \
//                                * filter_weight[81 * k + 9 * l + m];
// 						}
// 					}
// 				}
// 			}

// 			//add relu activation after conv
// 			output_im[i * output_w + j] = (tmp > 0.0) ? tmp : 0.0;
// 		}
// 	}
// }

// //5x5 convolution layer
// //output one feature map per kernel
// __kernel void conv2d5x5(
// 	const int input_channels, const int input_h, const int input_w,
// 	const int pad, const int stride,
// 	const int start_channel, 
// 	const int output_h, const int output_w,
// 	__global float* input_im,
// 	__global const float* filter_weight,
// 	__global const float* filter_bias,
// 	__global float *restrict output_im
// 	)
// {
// 	int filter_index = get_global_id(0); //get output channel index
// 	int i =  get_global_id(1);

// 	filter_weight += filter_index * input_channels * 25;
// 	float bias = filter_bias[filter_index];
// 	output_im += (start_channel + filter_index) * output_h * output_w;
	
// 	//loop over output feature map
// 	// for(int i = 0; i < output_h; i++)
// 	{
// 		for(int j = 0; j < output_w; j++)
// 		{
// 			//compute one element in the output feature map
// 			float tmp = bias;
			
// 			//compute dot product of 2 input_channels x 5 x 5 matrix
// 			for(int k = 0; k < input_channels; k++)
// 			{
// 				#pragma unroll
// 				for(int l = 0; l < 5; l++)
// 				{
// 					int h = i * stride + l - pad;
// 					for(int m = 0; m < 5; m++)
// 					{
// 						int w = j * stride + m - pad;
// 						if((h >= 0) && (h < input_h) && (w >= 0) && (w < input_w))
// 						{
// 							tmp += input_im[k * input_h * input_w + h * input_w + w] \
//                                * filter_weight[25 * k + 5 * l + m];
// 						}
// 					}
// 				}
// 			}

// 			//add relu activation after conv
// 			output_im[i * output_w + j] = (tmp > 0.0) ? tmp : 0.0;
// 		}
// 	}
// }

//3x3 convolution layer
//output one feature map per kernel
__kernel void conv2d3x3(
	const int input_channels, const int input_h, const int input_w,
	const int pad, const int stride,
	const int start_channel, //start_channel is for 1x1 feature map in fire layer
	const int output_h, const int output_w,
	__global float* input_im,
	__global const float* filter_weight,
	__global const float* filter_bias,
	__global float *restrict output_im
	)
{
	int filter_index = get_global_id(0); //get output channel index
	int i =  get_global_id(1);

	filter_weight += filter_index * input_channels * 9;
	float bias = filter_bias[filter_index];
	output_im += (start_channel + filter_index) * output_h * output_w;
	
	//loop over output feature map
	for(int i = 0; i < output_h; i++)
	{
		for(int j = 0; j < output_w; j++)
		{
			//compute one element in the output feature map
			float tmp = bias;
			
			//compute dot product of 2 input_channels x 3 x 3 matrix
			for(int k = 0; k < input_channels; k++)
			{
				#pragma unroll
				for(int l = 0; l < 3; l++)
				{
					int h = i * stride + l - pad;
					for(int m = 0; m < 3; m++)
					{
						int w = j * stride + m - pad;
						if((h >= 0) && (h < input_h) && (w >= 0) && (w < input_w))
						{
							tmp += input_im[k * input_h * input_w + h * input_w + w] \
                               * filter_weight[9 * k + 3 * l + m];
						}
					}
				}
			}

			//add relu activation after conv
			output_im[i * output_w + j] = (tmp > 0.0) ? tmp : 0.0;
		}
	}
}

// Dense layer
__kernel void dense(
	const int input_size,
	const int output_size,
	__global float* input_im,
	__global const float* filter_weight,
	__global const float* filter_bias,
	__global float *restrict output_im
	)
{
	int i = get_global_id(0);
	//loop over output feature map
	// for(int i = 0; i < output_size; i++)
	{
		output_im[i] = filter_bias[i];
		for(int j = 0; j < input_size; j++)
		{
			output_im[i] = output_im[i] + input_im[j]*filter_weight[output_size*i+j];
		}
	}
}
