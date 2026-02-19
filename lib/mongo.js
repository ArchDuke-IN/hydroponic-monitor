const { MongoClient, ServerApiVersion } = require('mongodb');

let uri = process.env.DATABASE_CONNECTION_STRING;
if (!uri) {
  console.error('DATABASE_CONNECTION_STRING environment variable is not set');
  throw new Error('DATABASE_CONNECTION_STRING environment variable is not set');
}

// Ensure SSL parameters are in the connection string for Vercel compatibility
if (!uri.includes('ssl=true')) {
  const separator = uri.includes('?') ? '&' : '?';
  uri += `${separator}ssl=true&sslValidate=false&retryWrites=true&w=majority`;
}

const client = new MongoClient(uri, {
  serverApi: {
    version: ServerApiVersion.v1,
    strict: true,
    deprecationErrors: true,
  },
  // Minimal configuration for serverless
  connectTimeoutMS: 60000,
  serverSelectionTimeoutMS: 60000,
  maxPoolSize: 1,
});

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
