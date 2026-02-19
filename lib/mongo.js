const { MongoClient, ServerApiVersion } = require('mongodb');

// Get connection string and ensure it has SSL disabled for Vercel compatibility
let uri = process.env.DATABASE_CONNECTION_STRING;
if (!uri) {
  console.error('DATABASE_CONNECTION_STRING environment variable is not set');
  throw new Error('DATABASE_CONNECTION_STRING environment variable is not set');
}

// Modify connection string to disable SSL validation
if (uri.includes('mongodb+srv://')) {
  // For SRV connections, add SSL parameters to the query string
  const urlParts = uri.split('?');
  const baseUrl = urlParts[0];
  const existingParams = urlParts[1] || '';
  const sslParams = 'ssl=true&sslValidate=false&authSource=admin';
  
  if (existingParams) {
    uri = `${baseUrl}?${existingParams}&${sslParams}`;
  } else {
    uri = `${baseUrl}?${sslParams}`;
  }
}

const client = new MongoClient(uri);

let cachedDb = null;

async function getDb() {
  if (cachedDb) return cachedDb;

  try {
    console.log('Attempting to connect to MongoDB...');
    await client.connect();
    console.log('MongoDB connection successful');
    const dbName = process.env.DATABASE_NAME || 'hydroponic_monitor';
    console.log('Using database:', dbName);
    cachedDb = client.db(dbName);
    return cachedDb;
  } catch (error) {
    console.error('MongoDB connection failed:', error.message);
    throw error;
  }
}

module.exports = { getDb };
