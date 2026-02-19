const { MongoClient } = require('mongodb');

const uri = process.env.DATABASE_CONNECTION_STRING;
if (!uri) {
  console.error('DATABASE_CONNECTION_STRING environment variable is not set');
  throw new Error('DATABASE_CONNECTION_STRING environment variable is not set');
}

const client = new MongoClient(uri, {
  tls: true,
  tlsInsecure: true,
  authSource: 'admin'
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
