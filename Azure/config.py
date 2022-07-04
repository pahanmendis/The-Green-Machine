import os

settings = {
    'host': os.environ.get('ACCOUNT_HOST', 'host'),
    'master_key': os.environ.get('ACCOUNT_KEY', 'key'),
    'database_id': os.environ.get('COSMOS_DATABASE', 'ToDoList'),
    'container_id': os.environ.get('COSMOS_CONTAINER', 'Items'),
}