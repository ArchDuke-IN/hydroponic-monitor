const { MongoClient, ServerApiVersion } = require('mongodb');

const uri = process.env.DATABASE_CONNECTION_STRING;
if (!uri) {
  console.error('DATABASE_CONNECTION_STRING environment variable is not set');
  throw new Error('DATABASE_CONNECTION_STRING environment variable is not set');
}

const client = new MongoClient(uri, {
  serverApi: {
    version: ServerApiVersion.v1,
    strict: true,
    deprecationErrors: true,
  },
});

let cachedDb = null;

async function getDb() {
  if (cachedDb) return cachedDb;

  await client.connect();
  const dbName = process.env.DATABASE_NAME || 'hydroponic_monitor';
  cachedDb = client.db(dbName);
  return cachedDb;
}

module.exports = { getDb };
