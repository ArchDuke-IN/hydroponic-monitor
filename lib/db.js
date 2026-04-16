const { neon } = require('@neondatabase/serverless');

const sql = neon(process.env.DATABASE_CONNECTION_STRING);

module.exports = { sql };
