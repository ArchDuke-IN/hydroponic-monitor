/**
 * CORS and common response helpers
 */

function setCors(res) {
  res.setHeader('Access-Control-Allow-Origin', '*');
  res.setHeader('Access-Control-Allow-Methods', 'GET, POST, OPTIONS');
  res.setHeader('Access-Control-Allow-Headers', 'Content-Type');
}

function handleOptions(req, res) {
  // Always set CORS headers for every request
  setCors(res);
  
  if (req.method === 'OPTIONS') {
    res.status(200).end();
    return true;
  }
  return false;
}

function sendJSON(res, data, status = 200) {
  // setCors(res); // Already handled in handleOptions or main handler
  res.status(status).json(data);
}

function sendError(res, message, status = 400) {
  sendJSON(res, { success: false, error: message }, status);
}

module.exports = { setCors, handleOptions, sendJSON, sendError };
