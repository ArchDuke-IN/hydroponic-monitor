const { MongoClient, ServerApiVersion } = require('mongodb');

const uri = process.env.MONGODB_URI;
if (!uri) {
  throw new Error('MONGODB_URI environment variable is not set');
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
  const dbName = process.env.MONGODB_DB || 'hydroponic_monitor';
  cachedDb = client.db(dbName);
  return cachedDb;
}

module.exports = { getDb };
