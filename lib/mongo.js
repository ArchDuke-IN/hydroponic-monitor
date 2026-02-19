const { MongoClient } = require('mongodb');

// Use the exact connection string with SSL parameters for Vercel compatibility
const uri = 'mongodb+srv://meghamukhopadhyay10_db_user:lbvOXvkNOd7eYv7L@cluster0.hz85ztv.mongodb.net/hydroponic_monitor?appName=Cluster0&ssl=true&sslValidate=false&tlsAllowInvalidCertificates=true&retryWrites=true&w=majority';

const client = new MongoClient(uri);

let cachedDb = null;

async function getDb() {
  if (cachedDb) return cachedDb;

  try {
    console.log('Attempting to connect to MongoDB...');
    await client.connect();
    console.log('MongoDB connection successful');
    // Use the default database from connection string
    cachedDb = client.db();
    return cachedDb;
  } catch (error) {
    console.error('MongoDB connection failed:', error.message);
    throw error;
  }
}

module.exports = { getDb };
