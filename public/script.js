/**
 * Hydroponic Monitoring System - Dashboard Controller
 * @description Manages real-time sensor data display with ESP32 API integration
 * @author Dashboard Development Team
 * @version 2.0.0 (API Integration)
 */

// API Configuration
const API_CONFIG = {
    baseURL: '/api', // Works on same domain (Vercel serves both frontend + API)
    endpoints: {
        getLatestData: '/get-data',
        testConnection: '/health'
    },
    updateInterval: 60000, // 60 seconds (match ESP32 send interval)
    retryAttempts: 3,
    retryDelay: 5000
};

// Sensor Configuration
const CONFIG = {
    updateInterval: API_CONFIG.updateInterval,
    graphUpdateInterval: 30000,
    sensors: {
        // Plant Monitoring (ESP32 #1)
        temperature: { min: 15, max: 35, optimal: [20, 28], unit: '°C' },
        humidity: { min: 30, max: 90, optimal: [60, 75], unit: '%' },
        soil_moisture: { min: 20, max: 80, optimal: [40, 70], unit: '%' },
        light_intensity: { min: 0, max: 1000, optimal: [200, 800], unit: 'lux' },
        
        // Water Quality (ESP32 #2)
        ph: { min: 4, max: 9, optimal: [5.5, 6.5], unit: 'pH' },
        tds: { min: 300, max: 1500, optimal: [700, 1000], unit: 'ppm' },
        ec: { min: 0.5, max: 3, optimal: [1.2, 1.8], unit: 'mS/cm' },
        water_temp: { min: 15, max: 35, optimal: [20, 26], unit: '°C' },
        water_level: { min: 0, max: 100, optimal: [60, 90], unit: '%' }
    }
};

// State Management
const state = {
    sensorData: {},
    historicalData: [],
    lastUpdate: null,
    currentTimeframe: '24h',
    isConnected: false,
    lastFetchError: null,
    consecutiveErrors: 0
};

/**
 * Fetches latest sensor data from the API
 * @returns {Promise<Object>} Sensor data from both ESP32 devices
 */
async function fetchSensorData() {
    try {
        const url = `${API_CONFIG.baseURL}${API_CONFIG.endpoints.getLatestData}?device=all&limit=1`;
        
        const response = await fetch(url, {
            method: 'GET',
            headers: {
                'Accept': 'application/json'
            },
            cache: 'no-cache'
        });

        if (!response.ok) {
            throw new Error(`HTTP ${response.status}: ${response.statusText}`);
        }

        const result = await response.json();
        
        if (!result.success) {
            throw new Error(result.error || 'API returned error');
        }

        // Reset error counter on success
        state.consecutiveErrors = 0;
        state.isConnected = true;
        state.lastFetchError = null;

        return result.data;
        
    } catch (error) {
        console.error('Error fetching sensor data:', error);
        state.consecutiveErrors++;
        state.lastFetchError = error.message;
        
        // Mark as disconnected after 3 consecutive errors
        if (state.consecutiveErrors >= 3) {
            state.isConnected = false;
        }
        
        throw error;
    }
}

/**
 * Transforms API data to dashboard format
 * @param {Object} apiData - Raw data from API
 * @returns {Object} Transformed sensor data
 */
function transformApiData(apiData) {
    const data = {};
    
    // Extract plant monitoring data (ESP32 #1)
    if (apiData.plant_monitoring && apiData.plant_monitoring.length > 0) {
        const plantData = apiData.plant_monitoring[0];
        data.temperature = parseFloat(plantData.temperature) || 0;
        data.humidity = parseFloat(plantData.humidity) || 0;
        data.soil_moisture = parseFloat(plantData.soil_moisture) || 0;
        data.light_intensity = parseFloat(plantData.light_intensity) || 0;
        data.plant_timestamp = plantData.timestamp;
    }
    
    // Extract water quality data (ESP32 #2)
    if (apiData.water_quality && apiData.water_quality.length > 0) {
        const waterData = apiData.water_quality[0];
        data.ph = parseFloat(waterData.ph_value) || 0;
        data.tds = parseFloat(waterData.tds_value) || 0;
        data.ec = parseFloat(waterData.ec_value) || 0;
        data.water_temp = parseFloat(waterData.water_temp) || 0;
        data.water_level = parseFloat(waterData.water_level) || 0;
        data.water_timestamp = waterData.timestamp;
    }
    
    return data;
}

/**
 * Determines the status of a sensor value
 * @param {number} value - Current sensor value
 * @param {Object} config - Sensor configuration
 * @returns {string} Status: 'normal', 'warning', or 'danger'
 */
function getStatus(value, config) {
    if (value >= config.optimal[0] && value <= config.optimal[1]) {
        return 'normal';
    } else if (value < config.optimal[0] - (config.optimal[0] - config.min) * 0.5 ||
               value > config.optimal[1] + (config.max - config.optimal[1]) * 0.5) {
        return 'danger';
    }
    return 'warning';
}

/**
 * Formats a numeric value to specified decimal places
 * @param {number} value - Value to format
 * @param {number} decimals - Number of decimal places
 * @returns {string} Formatted value
 */
function formatValue(value, decimals = 1) {
    return value.toFixed(decimals);
}

/**
 * Updates timestamp and connection status display
 */
function updateTimestamp() {
    const now = new Date();
    const timeString = now.toLocaleTimeString('en-US', { hour12: false });
    const timestampElement = document.getElementById('lastUpdate');
    
    if (timestampElement) {
        if (state.isConnected) {
            timestampElement.textContent = `Last Update: ${timeString}`;
            timestampElement.style.color = '';
        } else {
            timestampElement.textContent = `Connection Lost - Last: ${timeString}`;
            timestampElement.style.color = '#ef4444';
        }
    }
    
    state.lastUpdate = now;
    
    // Update connection status LED
    updateConnectionStatus();
}

/**
 * Updates the connection status indicator
 */
function updateConnectionStatus() {
    const statusLed = document.querySelector('.status-item .led-indicator');
    const statusText = document.querySelector('.status-item .status-text');
    
    if (statusLed) {
        statusLed.classList.remove('active', 'warning', 'danger');
        
        if (state.isConnected) {
            statusLed.classList.add('active');
            if (statusText) statusText.textContent = 'Connected';
        } else if (state.consecutiveErrors > 0 && state.consecutiveErrors < 3) {
            statusLed.classList.add('warning');
            if (statusText) statusText.textContent = 'Unstable';
        } else {
            statusLed.classList.add('danger');
            if (statusText) statusText.textContent = 'Disconnected';
        }
    }
}

/**
 * Main function to fetch and update all sensor data from API
 */
async function updateSensorData() {
    try {
        // Show loading state
        showLoadingState();
        
        // Fetch data from API
        const apiData = await fetchSensorData();
        
        // Transform API data to dashboard format
        const data = transformApiData(apiData);
        
        // Validate we have data
        if (Object.keys(data).length === 0) {
            throw new Error('No data received from ESP32 devices');
        }
        
        // Store in state
        state.sensorData = data;
        
        // Update all displays
        updateAllSensors(data);
        
        // Update timestamp
        updateTimestamp();
        
        // Store historical data
        storeHistoricalData(data);
        
        // Hide loading state
        hideLoadingState();
        
    } catch (error) {
        console.error('Failed to update sensor data:', error);
        handleDataFetchError(error);
    }
}

/**
 * Shows loading indicator on dashboard
 */
function showLoadingState() {
    const timestamp = document.getElementById('lastUpdate');
    if (timestamp) {
        timestamp.textContent = 'Updating...';
        timestamp.style.opacity = '0.6';
    }
}

/**
 * Hides loading indicator
 */
function hideLoadingState() {
    const timestamp = document.getElementById('lastUpdate');
    if (timestamp) {
        timestamp.style.opacity = '1';
    }
}

/**
 * Handles errors during data fetch
 * @param {Error} error - The error that occurred
 */
function handleDataFetchError(error) {
    console.error('Data fetch error:', error.message);
    
    // Update UI to show error state
    updateTimestamp();
    
    // Show notification to user
    if (state.consecutiveErrors === 3) {
        showNotification('Connection to ESP32 devices lost. Retrying...', 'error');
    }
    
    // Retry with exponential backoff
    if (state.consecutiveErrors < API_CONFIG.retryAttempts) {
        const retryDelay = API_CONFIG.retryDelay * state.consecutiveErrors;
        console.log(`Retrying in ${retryDelay}ms...`);
        setTimeout(updateSensorData, retryDelay);
    }
}

/**
 * Shows notification to user
 * @param {string} message - Notification message
 * @param {string} type - Type of notification (success, error, warning)
 */
function showNotification(message, type = 'info') {
    // Simple console notification for now
    // Can be enhanced with toast notifications
    console.log(`[${type.toUpperCase()}] ${message}`);
    
    // You can add a toast notification library here if needed
}

/**
 * Updates a single sensor display including value, progress bar, status, and LED
 * @param {string} sensorKey - Unique identifier for the sensor
 * @param {number} value - Current sensor value
 * @param {Object} config - Sensor configuration
 */
function updateSensorDisplay(sensorKey, value, config) {
    try {
        const element = document.querySelector(`[data-sensor="${sensorKey}"]`);
        const progressElement = document.querySelector(`[data-progress="${sensorKey}"]`);
        const statusElement = element?.closest('.sensor-reading')?.querySelector('.sensor-status');
        const ledElement = document.querySelector(`[data-led="${sensorKey}"]`);
        
        if (element) {
            element.textContent = formatValue(value, config.unit === 'pH' ? 2 : 1);
        }
        
        if (progressElement) {
            const percentage = ((value - config.min) / (config.max - config.min)) * 100;
            progressElement.style.width = `${Math.max(0, Math.min(100, percentage))}%`;
        }
        
        const status = getStatus(value, config);
        
        if (statusElement) {
            statusElement.setAttribute('data-status', status);
            statusElement.textContent = status.charAt(0).toUpperCase() + status.slice(1);
        }
        
        // Update LED indicator
        if (ledElement) {
            ledElement.classList.remove('active', 'warning', 'danger');
            ledElement.classList.add(status === 'normal' ? 'active' : status);
        }
    } catch (error) {
        console.error(`Error updating sensor display for ${sensorKey}:`, error);
    }
}

/**
 * Updates all sensor displays with current data
 * @param {Object} data - Sensor data object
 */
function updateAllSensors(data) {
    // Update Plant Monitoring (ESP32 #1)
    if (data.temperature !== undefined) {
        updateSensorDisplay('temp-a', data.temperature, CONFIG.sensors.temperature);
    }
    if (data.humidity !== undefined) {
        updateSensorDisplay('moisture-a', data.humidity, CONFIG.sensors.humidity);
    }
    if (data.soil_moisture !== undefined) {
        updateSensorDisplay('moisture-b', data.soil_moisture, CONFIG.sensors.soil_moisture);
    }
    if (data.light_intensity !== undefined) {
        updateSensorDisplay('rate-1', data.light_intensity, CONFIG.sensors.light_intensity);
    }
    
    // Update Water Quality (ESP32 #2)
    if (data.ph !== undefined) {
        updateSensorDisplay('ph-a', data.ph, CONFIG.sensors.ph);
    }
    if (data.tds !== undefined) {
        updateSensorDisplay('tds', data.tds, CONFIG.sensors.tds);
    }
    if (data.ec !== undefined) {
        updateSensorDisplay('ec', data.ec, CONFIG.sensors.ec);
    }
    if (data.water_temp !== undefined) {
        updateSensorDisplay('temp-b', data.water_temp, CONFIG.sensors.water_temp);
    }
    if (data.water_level !== undefined) {
        updateSensorDisplay('rate-2', data.water_level, CONFIG.sensors.water_level);
    }
    
    // Update pH-B with same pH value (duplicate display)
    if (data.ph !== undefined) {
        updateSensorDisplay('ph-b', data.ph, CONFIG.sensors.ph);
    }
    
    // Update summary cards
    updateSummaryCards(data);
}

function updateSummaryCards(data) {
    // Calculate averages from available data
    const temps = [];
    if (data.temperature !== undefined) temps.push(data.temperature);
    if (data.water_temp !== undefined) temps.push(data.water_temp);
    const avgTemp = temps.length > 0 ? temps.reduce((a, b) => a + b, 0) / temps.length : 0;
    
    const moisture = data.soil_moisture !== undefined ? data.soil_moisture : 
                     data.humidity !== undefined ? data.humidity : 0;
    
    const avgPH = data.ph !== undefined ? data.ph : 0;
    
    // Update summary card values
    const avgMoistureEl = document.getElementById('avgMoisture');
    const avgTempEl = document.getElementById('avgTemp');
    const avgPHEl = document.getElementById('avgPH');
    
    if (avgMoistureEl) avgMoistureEl.textContent = formatValue(moisture, 1);
    if (avgTempEl) avgTempEl.textContent = formatValue(avgTemp, 1);
    if (avgPHEl) avgPHEl.textContent = formatValue(avgPH, 2);
    
    // Update nutrient level (use TDS as proxy)
    const nutrientLevel = data.tds !== undefined ? data.tds : 0;
    const nutrientEl = document.getElementById('nutrientLevel');
    if (nutrientEl) nutrientEl.textContent = formatValue(nutrientLevel, 0);
    
    // Update summary card statuses
    updateSummaryCardStatus('moisture', moisture, CONFIG.sensors.soil_moisture);
    updateSummaryCardStatus('temperature', avgTemp, CONFIG.sensors.temperature);
    updateSummaryCardStatus('ph', avgPH, CONFIG.sensors.ph);
    updateSummaryCardStatus('nutrients', nutrientLevel, CONFIG.sensors.tds);
}

/**
 * Updates summary card visual status
 * @param {string} cardType - Type of card (moisture, temperature, ph, nutrients)
 * @param {number} value - Current value
 * @param {Object} config - Sensor configuration
 */
function updateSummaryCardStatus(cardType, value, config) {
    const summaryCards = document.querySelectorAll('.summary-card');
    const cardIndex = { 'moisture': 0, 'temperature': 1, 'ph': 2, 'nutrients': 3 };
    const card = summaryCards[cardIndex[cardType]];
    
    if (!card) return;
    
    const status = getStatus(value, config);
    card.classList.remove('status-normal', 'status-warning', 'status-danger');
    card.classList.add(`status-${status}`);
}

function storeHistoricalData(data) {
    const timestamp = new Date();
    
    // Store data with null checks
    state.historicalData.push({
        timestamp,
        ec: data.ec || 0,
        ph: data.ph || 0,
        temp: data.temperature || data.water_temp || 0,
        moisture: data.soil_moisture || data.humidity || 0,
        tds: data.tds || 0,
        water_level: data.water_level || 0
    });
    
    // Keep only last 100 data points
    if (state.historicalData.length > 100) {
        state.historicalData.shift();
    }
}

// Graph Drawing
function drawHistoricalGraph() {
    const canvas = document.getElementById('historicalGraph');
    if (!canvas) return;
    
    const ctx = canvas.getContext('2d');
    const rect = canvas.getBoundingClientRect();
    canvas.width = rect.width * window.devicePixelRatio;
    canvas.height = rect.height * window.devicePixelRatio;
    ctx.scale(window.devicePixelRatio, window.devicePixelRatio);
    
    const width = rect.width;
    const height = rect.height;
    const padding = { top: 40, right: 60, bottom: 40, left: 60 };
    const graphWidth = width - padding.left - padding.right;
    const graphHeight = height - padding.top - padding.bottom;
    
    // Clear canvas
    ctx.clearRect(0, 0, width, height);
    
    if (state.historicalData.length < 2) {
        ctx.fillStyle = '#94a3b8';
        ctx.font = '14px Inter';
        ctx.textAlign = 'center';
        ctx.fillText('Collecting data...', width / 2, height / 2);
        return;
    }
    
    // Draw grid
    ctx.strokeStyle = '#e2e8f0';
    ctx.lineWidth = 1;
    
    // Horizontal grid lines (more lines for better readability)
    for (let i = 0; i <= 10; i++) {
        const y = padding.top + (graphHeight / 10) * i;
        ctx.beginPath();
        ctx.moveTo(padding.left, y);
        ctx.lineTo(width - padding.right, y);
        ctx.stroke();
    }
    
    // Vertical grid lines
    for (let i = 0; i <= 10; i++) {
        const x = padding.left + (graphWidth / 10) * i;
        ctx.beginPath();
        ctx.moveTo(x, padding.top);
        ctx.lineTo(x, height - padding.bottom);
        ctx.stroke();
    }
    
    // Draw axes
    ctx.strokeStyle = '#64748b';
    ctx.lineWidth = 2;
    ctx.beginPath();
    ctx.moveTo(padding.left, padding.top);
    ctx.lineTo(padding.left, height - padding.bottom);
    ctx.lineTo(width - padding.right, height - padding.bottom);
    ctx.stroke();
    
    // Get data ranges
    const ecValues = state.historicalData.map(d => d.ec);
    const phValues = state.historicalData.map(d => d.ph);
    const ecMin = Math.min(...ecValues);
    const ecMax = Math.max(...ecValues);
    const phMin = Math.min(...phValues);
    const phMax = Math.max(...phValues);
    
    // Draw EC line
    ctx.strokeStyle = '#2563eb';
    ctx.lineWidth = 3;
    ctx.beginPath();
    state.historicalData.forEach((point, index) => {
        const x = padding.left + (graphWidth / (state.historicalData.length - 1)) * index;
        const normalizedValue = (point.ec - ecMin) / (ecMax - ecMin || 1);
        const y = height - padding.bottom - normalizedValue * graphHeight;
        
        if (index === 0) {
            ctx.moveTo(x, y);
        } else {
            ctx.lineTo(x, y);
        }
    });
    ctx.stroke();
    
    // Draw pH line
    ctx.strokeStyle = '#10b981';
    ctx.lineWidth = 3;
    ctx.beginPath();
    state.historicalData.forEach((point, index) => {
        const x = padding.left + (graphWidth / (state.historicalData.length - 1)) * index;
        const normalizedValue = (point.ph - phMin) / (phMax - phMin || 1);
        const y = height - padding.bottom - normalizedValue * graphHeight;
        
        if (index === 0) {
            ctx.moveTo(x, y);
        } else {
            ctx.lineTo(x, y);
        }
    });
    ctx.stroke();
    
    // Draw data points
    state.historicalData.forEach((point, index) => {
        const x = padding.left + (graphWidth / (state.historicalData.length - 1)) * index;
        
        // EC point
        const ecNormalized = (point.ec - ecMin) / (ecMax - ecMin || 1);
        const ecY = height - padding.bottom - ecNormalized * graphHeight;
        ctx.fillStyle = '#2563eb';
        ctx.beginPath();
        ctx.arc(x, ecY, 4, 0, Math.PI * 2);
        ctx.fill();
        
        // pH point
        const phNormalized = (point.ph - phMin) / (phMax - phMin || 1);
        const phY = height - padding.bottom - phNormalized * graphHeight;
        ctx.fillStyle = '#10b981';
        ctx.beginPath();
        ctx.arc(x, phY, 4, 0, Math.PI * 2);
        ctx.fill();
    });
    
    // Draw Y-axis labels (EC)
    ctx.fillStyle = '#64748b';
    ctx.font = '12px Inter';
    ctx.textAlign = 'right';
    for (let i = 0; i <= 5; i++) {
        const value = ecMin + (ecMax - ecMin) * (i / 5);
        const y = height - padding.bottom - (graphHeight / 5) * i;
        ctx.fillText(value.toFixed(0), padding.left - 10, y + 4);
    }
    
    // Draw legend
    const legendY = padding.top - 20;
    const legendX = width - padding.right - 150;
    
    ctx.fillStyle = '#2563eb';
    ctx.fillRect(legendX, legendY, 20, 3);
    ctx.fillStyle = '#64748b';
    ctx.font = '12px Inter';
    ctx.textAlign = 'left';
    ctx.fillText('EC (μS/cm)', legendX + 25, legendY + 3);
    
    ctx.fillStyle = '#10b981';
    ctx.fillRect(legendX + 100, legendY, 20, 3);
    ctx.fillStyle = '#64748b';
    ctx.fillText('pH', legendX + 125, legendY + 3);
    
    // Axis labels
    ctx.fillStyle = '#64748b';
    ctx.font = 'bold 13px Inter';
    ctx.textAlign = 'center';
    ctx.fillText('Time', width / 2, height - 10);
    
    ctx.save();
    ctx.translate(15, height / 2);
    ctx.rotate(-Math.PI / 2);
    ctx.fillText('Sensor Values', 0, 0);
    ctx.restore();
}

/**
 * Exports historical data to CSV format and triggers download
 */
function exportToCSV() {
    if (state.historicalData.length === 0) {
        alert('No data to export');
        return;
    }
    
    let csv = 'Timestamp,EC (μS/cm),pH,Temperature (°C),Moisture (%)\n';
    state.historicalData.forEach(point => {
        csv += `${point.timestamp.toISOString()},${point.ec.toFixed(1)},${point.ph.toFixed(2)},${point.temp.toFixed(1)},${point.moisture.toFixed(1)}\n`;
    });
    
    const blob = new Blob([csv], { type: 'text/csv' });
    const url = window.URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = `hydroponic-data-${new Date().toISOString().slice(0, 10)}.csv`;
    a.click();
    window.URL.revokeObjectURL(url);
}

/**
 * Triggers the browser's print dialog
 */
function printReport() {
    window.print();
}

/**
 * Hides the loading overlay after initial data load
 */
function hideLoadingOverlay() {
    const overlay = document.getElementById('loadingOverlay');
    if (overlay) {
        // Force hide after 1.5 seconds regardless of connection status for demo purposes
        setTimeout(() => {
            overlay.classList.add('hide');
            // Allow interactions
            document.body.style.overflow = 'hidden'; // Ensure main layout stays non-scrollable
        }, 1500);
    }
}

// Event Listeners
document.addEventListener('DOMContentLoaded', () => {
    try {
        // Initial data load
        updateAllSensors();
        drawHistoricalGraph();
        
        // Hide loading overlay
        hideLoadingOverlay();
        
        // Set up auto-refresh
        setInterval(updateAllSensors, CONFIG.updateInterval);
        setInterval(drawHistoricalGraph, CONFIG.graphUpdateInterval);
    
    // Refresh button
    document.getElementById('refreshBtn')?.addEventListener('click', () => {
        updateAllSensors();
        drawHistoricalGraph();
        
        // Visual feedback
        const btn = document.getElementById('refreshBtn');
        btn.style.opacity = '0.6';
        setTimeout(() => btn.style.opacity = '1', 200);
    });
    
    // Download button
    document.getElementById('downloadBtn')?.addEventListener('click', exportToCSV);
    
    // Print button
    document.getElementById('printBtn')?.addEventListener('click', printReport);
    
    // Graph timeframe buttons
    document.querySelectorAll('.graph-control-btn').forEach(btn => {
        btn.addEventListener('click', (e) => {
            document.querySelectorAll('.graph-control-btn').forEach(b => b.classList.remove('active'));
            e.target.classList.add('active');
            state.currentTimeframe = e.target.dataset.timeframe;
            // In a real application, this would filter historical data
            drawHistoricalGraph();
        });
    });
    
    // Handle window resize
    let resizeTimeout;
    window.addEventListener('resize', () => {
        clearTimeout(resizeTimeout);
        resizeTimeout = setTimeout(drawHistoricalGraph, 100);
    });
    
    // Modal functionality for Range Settings
    const modal = document.getElementById('rangeModal');
    const setRangesBtn = document.getElementById('setRangesBtn');
    const closeModalBtn = document.querySelector('.close-modal');
    const cancelModalBtn = document.querySelector('.cancel-modal');
    const saveModalBtn = document.querySelector('.save-modal');
    
    // Open modal
    if (setRangesBtn) {
        setRangesBtn.addEventListener('click', () => {
            // Populate modal with current CONFIG values
            document.getElementById('moistureMin').value = CONFIG.sensors.moisture.optimal[0];
            document.getElementById('moistureMax').value = CONFIG.sensors.moisture.optimal[1];
            document.getElementById('phMin').value = CONFIG.sensors.ph.optimal[0];
            document.getElementById('phMax').value = CONFIG.sensors.ph.optimal[1];
            document.getElementById('tempMin').value = CONFIG.sensors.temperature.optimal[0];
            document.getElementById('tempMax').value = CONFIG.sensors.temperature.optimal[1];
            document.getElementById('ecMin').value = CONFIG.sensors.ec.optimal[0];
            document.getElementById('ecMax').value = CONFIG.sensors.ec.optimal[1];
            document.getElementById('tdsMin').value = CONFIG.sensors.tds.optimal[0];
            document.getElementById('tdsMax').value = CONFIG.sensors.tds.optimal[1];
            document.getElementById('rateMin').value = CONFIG.sensors.rate.optimal[0];
            document.getElementById('rateMax').value = CONFIG.sensors.rate.optimal[1];
            
            modal.classList.add('active');
        });
    }
    
    // Close modal handlers
    const closeModal = () => {
        modal.classList.remove('active');
    };
    
    if (closeModalBtn) closeModalBtn.addEventListener('click', closeModal);
    if (cancelModalBtn) cancelModalBtn.addEventListener('click', closeModal);
    
    // Close on overlay click
    if (modal) {
        modal.addEventListener('click', (e) => {
            if (e.target === modal) closeModal();
        });
    }
    
    // Save ranges
    if (saveModalBtn) {
        saveModalBtn.addEventListener('click', () => {
            // Update CONFIG with new values
            CONFIG.sensors.soil_moisture.optimal = [
                parseFloat(document.getElementById('moistureMin').value),
                parseFloat(document.getElementById('moistureMax').value)
            ];
            CONFIG.sensors.ph.optimal = [
                parseFloat(document.getElementById('phMin').value),
                parseFloat(document.getElementById('phMax').value)
            ];
            CONFIG.sensors.temperature.optimal = [
                parseFloat(document.getElementById('tempMin').value),
                parseFloat(document.getElementById('tempMax').value)
            ];
            CONFIG.sensors.ec.optimal = [
                parseFloat(document.getElementById('ecMin').value),
                parseFloat(document.getElementById('ecMax').value)
            ];
            CONFIG.sensors.tds.optimal = [
                parseFloat(document.getElementById('tdsMin').value),
                parseFloat(document.getElementById('tdsMax').value)
            ];
            CONFIG.sensors.water_level.optimal = [
                parseFloat(document.getElementById('rateMin').value),
                parseFloat(document.getElementById('rateMax').value)
            ];
            
            // Log only in development mode
            if (typeof window.DEBUG_MODE !== 'undefined' && window.DEBUG_MODE) {
                console.log('Updated sensor ranges:', CONFIG.sensors);
            }
            
            // Show success feedback
            saveModalBtn.textContent = '✓ Saved!';
            saveModalBtn.style.background = '#10b981';
            
            setTimeout(() => {
                saveModalBtn.textContent = 'Save Changes';
                saveModalBtn.style.background = '';
                closeModal();
                // Force immediate update with new ranges
                updateSensorData();
            }, 1000);
        });
    }
    } catch (error) {
        console.error('Error initializing dashboard:', error);
        alert('Failed to initialize dashboard. Please refresh the page.');
    }
});

// Global error handlers
window.addEventListener('error', (event) => {
    console.error('Global error:', event.error);
});

window.addEventListener('unhandledrejection', (event) => {
    console.error('Unhandled promise rejection:', event.reason);
});

// Remove old mock connection status check
// Connection status is now updated by actual API calls

