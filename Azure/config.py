import os

settings = {
    'host': os.environ.get('ACCOUNT_HOST', 'https://ap116-sql-data.documents.azure.com:443/'),
    'master_key': os.environ.get('ACCOUNT_KEY', 'PUMtMoawiui1XdquF7X1eb0LU75bPa7Qb1iTWFjbO7qPlhPTKx9S5f3j7eTpKNrP4c0FLmFnqTNiZlDcZGVG6w=='),
    'database_id': os.environ.get('COSMOS_DATABASE', 'ToDoList'),
    'container_id': os.environ.get('COSMOS_CONTAINER', 'Items'),
}