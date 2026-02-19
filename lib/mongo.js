const { MongoClient, ServerApiVersion } = require('mongodb');

const uri = process.env.DATABASE_CONNECTION_STRING;
if (!uri) {
  console.error('DATABASE_CONNECTION_STRING environment variable is not set');
  throw new Error('DATABASE_CONNECTION_STRING environment variable is not set');
}

// MongoDB client with serverless-friendly configuration
const client = new MongoClient(uri, {
  serverApi: {
    version: ServerApiVersion.v1,
    strict: true,
    deprecationErrors: true,
  },
  // Connection pool settings for serverless
  maxPoolSize: 1,
  maxIdleTimeMS: 30000,
  serverSelectionTimeoutMS: 5000,
  socketTimeoutMS: 45000,
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
