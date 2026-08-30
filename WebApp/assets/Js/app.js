// Common JavaScript utilities for ESP32 Webserver

class ESP32WebAPI {
    constructor(baseUrl = '') {
        this.baseUrl = baseUrl;
    }

    // Returns the stored auth token, or null
    _token() {
        return localStorage.getItem('authToken');
    }

    async request(endpoint, options = {}) {
        const headers = { 'Content-Type': 'application/json' };
        const token = this._token();
        if (token) headers['X-Auth-Token'] = token;

        const config = { headers, ...options };

        try {
            const response = await fetch(this.baseUrl + endpoint, config);
            // Unauthorised — wipe token and redirect to login (except on login page itself)
            if (response.status === 401 && !window.location.pathname.startsWith('/login')) {
                localStorage.removeItem('authToken');
                window.location.replace('/login');
                return { success: false, error: 'Unauthorized' };
            }
            const data = await response.json();
            return { success: true, data };
        } catch (error) {
            console.error('API Request failed:', error);
            return { success: false, error: error.message };
        }
    }

    async get(endpoint) {
        return this.request(endpoint, { method: 'GET' });
    }

    async post(endpoint, data) {
        return this.request(endpoint, {
            method: 'POST',
            body: JSON.stringify(data),
        });
    }

    // Auth API methods
    async login(password) {
        return this.post('/api/login', { password });
    }

    async logout() {
        return this.post('/api/logout', {});
    }

    async reboot() {
        return this.post('/api/reboot', {});
    }

    async changePassword(current, newPassword) {
        return this.post('/api/auth/change-password', { current, 'new': newPassword });
    }
    async setInitialPassword(password) {
        return this.post('/api/auth/set-initial-password', { password });
    }

    async getSettings() {
        return this.get('/api/settings');
    }

    async saveSettings(controllerName, logoutMinutes) {
        return this.post('/api/settings/save', { controllerName, logoutMinutes });
    }

    async getSettingsBackup() { return this.get('/api/settings/backup'); }
    async restoreSettings(data) { return this.post('/api/settings/restore', data); }
    async factoryReset(password) { return this.post('/api/settings/factory-reset', { password }); }

    // WiFi API methods
    async getWiFiStatus() {
        return this.get('/api/wifi/status');
    }

    async scanWiFiNetworks() {
        return this.get('/api/wifi/scan');
    }

    async connectToWiFi(ssid, password) {
        return this.post('/api/wifi/connect', { ssid, password });
    }

    // Device config (allowed pins + max devices)
    async getDeviceConfig() {
        return this.get('/api/device-config');
    }

    // Device API methods
    async getDevices() {
        return this.get('/api/devices');
    }

    async addDevice(name, pin, voltage) {
        return this.post('/api/devices/add', { name, pin, voltage });
    }

    async toggleDevice(index, state) {
        return this.post('/api/devices/toggle', { index, state });
    }

    async removeDevice(index) {
        return this.post('/api/devices/remove', { index });
    }
}

// Utility functions
const Utils = {
    showLoading(elementId) {
        const element = document.getElementById(elementId);
        if (element) {
            element.innerHTML = `
                <div class="loading">
                    <div class="loading-spinner"></div>
                    <div>Loading...</div>
                </div>
            `;
        }
    },

    hideLoading(elementId) {
        const element = document.getElementById(elementId);
        if (element) {
            element.style.display = 'none';
        }
    },

    showAlert(message, type = 'info', containerId = 'alertContainer') {
        const container = document.getElementById(containerId);
        if (!container) return;

        const alertDiv = document.createElement('div');
        alertDiv.className = `alert alert-${type}`;
        alertDiv.innerHTML = `
            ${message}
            <button type="button" onclick="this.parentElement.remove()" style="float: right; background: none; border: none; font-size: 18px; cursor: pointer;">&times;</button>
        `;

        container.appendChild(alertDiv);

        // Auto-remove after 5 seconds
        setTimeout(() => {
            if (alertDiv.parentElement) {
                alertDiv.remove();
            }
        }, 5000);
    },

    formatBytes(bytes) {
        if (bytes === 0) return '0 Bytes';
        const k = 1024;
        const sizes = ['Bytes', 'KB', 'MB', 'GB'];
        const i = Math.floor(Math.log(bytes) / Math.log(k));
        return parseFloat((bytes / Math.pow(k, i)).toFixed(2)) + ' ' + sizes[i];
    },

    getSignalStrength(rssi) {
        if (rssi >= -30) return { level: 'excellent', text: 'Excellent' };
        if (rssi >= -50) return { level: 'good', text: 'Good' };
        if (rssi >= -70) return { level: 'fair', text: 'Fair' };
        return { level: 'poor', text: 'Poor' };
    },

    debounce(func, wait) {
        let timeout;
        return function executedFunction(...args) {
            const later = () => {
                clearTimeout(timeout);
                func(...args);
            };
            clearTimeout(timeout);
            timeout = setTimeout(later, wait);
        };
    }
};

// Form validation utilities
const FormValidator = {
    validateEmail(email) {
        const regex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;
        return regex.test(email);
    },

    validatePassword(password) {
        return password.length >= 6;
    },

    validateUsername(username) {
        return username.length >= 3 && /^[a-zA-Z0-9_]+$/.test(username);
    },

    showFieldError(fieldId, message) {
        const field = document.getElementById(fieldId);
        if (!field) return;

        // Remove existing error
        const existingError = field.parentElement.querySelector('.field-error');
        if (existingError) {
            existingError.remove();
        }

        // Add new error
        const errorDiv = document.createElement('div');
        errorDiv.className = 'field-error';
        errorDiv.style.color = '#dc3545';
        errorDiv.style.fontSize = '14px';
        errorDiv.style.marginTop = '5px';
        errorDiv.textContent = message;

        field.parentElement.appendChild(errorDiv);
        field.style.borderColor = '#dc3545';
    },

    clearFieldError(fieldId) {
        const field = document.getElementById(fieldId);
        if (!field) return;

        const error = field.parentElement.querySelector('.field-error');
        if (error) {
            error.remove();
        }
        field.style.borderColor = '#e1e5e9';
    }
};

// WiFi management utilities
const WiFiManager = {
    api: new ESP32WebAPI(),

    async displayWiFiNetworks(containerId) {
        const container = document.getElementById(containerId);
        if (!container) return;

        Utils.showLoading(containerId);

        const result = await this.api.scanWiFiNetworks();

        if (result.success && result.data.networks) {
            container.innerHTML = this.renderWiFiList(result.data.networks);
        } else {
            container.innerHTML = '<div class="alert alert-danger">Failed to scan WiFi networks</div>';
        }
    },

    renderWiFiList(networks) {
        if (!networks.length) {
            return '<div class="alert alert-info">No WiFi networks found</div>';
        }

        const listHTML = networks.map(network => {
            const signal = Utils.getSignalStrength(network.rssi);
            const securityIcon = network.encrypted ? '🔒' : '🔓';

            return `
                <div class="wifi-item" onclick="WiFiManager.selectNetwork('${network.ssid}', ${network.encrypted})">
                    <div>
                        <strong>${network.ssid}</strong>
                        <span style="margin-left: 10px;">${securityIcon}</span>
                    </div>
                    <div class="wifi-signal ${signal.level}">
                        ${signal.text} (${network.rssi} dBm)
                    </div>
                </div>
            `;
        }).join('');

        return `<div class="wifi-list">${listHTML}</div>`;
    },

    selectNetwork(ssid, encrypted) {
        // Remove previous selection
        document.querySelectorAll('.wifi-item').forEach(item => {
            item.classList.remove('selected');
        });

        // Add selection to clicked item
        event.target.closest('.wifi-item').classList.add('selected');

        // Show connection form
        this.showConnectionForm(ssid, encrypted);
    },

    showConnectionForm(ssid, encrypted) {
        const formHTML = `
            <div class="form-group">
                <label>Selected Network:</label>
                <input type="text" class="form-control" value="${ssid}" readonly>
            </div>
            ${encrypted ? `
                <div class="form-group">
                    <label for="wifiPassword">Password:</label>
                    <input type="password" id="wifiPassword" class="form-control" required>
                </div>
            ` : ''}
            <button type="button" class="btn btn-primary" onclick="WiFiManager.connectToNetwork('${ssid}', ${encrypted})">
                Connect
            </button>
        `;

        const formContainer = document.getElementById('connectionForm');
        if (formContainer) {
            formContainer.innerHTML = formHTML;
            formContainer.style.display = 'block';
        }
    },

    async connectToNetwork(ssid, encrypted) {
        const password = encrypted ? document.getElementById('wifiPassword').value : '';

        if (encrypted && !password) {
            Utils.showAlert('Password is required for encrypted networks', 'danger');
            return;
        }

        const button = event.target;
        const originalText = button.textContent;
        button.textContent = 'Connecting...';
        button.disabled = true;

        const result = await this.api.connectToWiFi(ssid, password);

        if (result.success && result.data.success) {
            Utils.showAlert(`Successfully connected to ${ssid}`, 'success');
            // Redirect to home page after successful connection
            setTimeout(() => {
                window.location.href = '/';
            }, 2000);
        } else {
            const message = result.data?.message || 'Failed to connect to network';
            Utils.showAlert(message, 'danger');
        }

        button.textContent = originalText;
        button.disabled = false;
    }
};

const DEFAULT_SESSION_INACTIVITY_MS = 15 * 60 * 1000;

function redirectToLogin() {
    localStorage.removeItem('authToken');
    localStorage.removeItem('lastActivity');
    if (!window.location.pathname.startsWith('/login')) {
        window.location.replace('/login');
    }
}

function installSessionGuard() {
    if (window.location.pathname.startsWith('/login')) return;
    if (!localStorage.getItem('authToken')) {
        redirectToLogin();
        return;
    }

    let timer;
    function armTimer() {
        clearTimeout(timer);
        const lastActivity = Number(localStorage.getItem('lastActivity'));
        const minutes = Number(localStorage.getItem('logoutMinutes'));
        const timeout = (Number.isFinite(minutes) && minutes >= 1 ? minutes * 60 * 1000 : DEFAULT_SESSION_INACTIVITY_MS);
        const remaining = timeout - (Date.now() - lastActivity);
        if (!Number.isFinite(lastActivity) || remaining <= 0) {
            redirectToLogin();
            return;
        }
        timer = setTimeout(redirectToLogin, remaining);
    }

    function recordActivity() {
        localStorage.setItem('lastActivity', String(Date.now()));
        armTimer();
    }

    ['pointerdown', 'keydown', 'touchstart'].forEach(eventName => {
        document.addEventListener(eventName, recordActivity, { passive: true });
    });
    window.addEventListener('pageshow', armTimer);
    armTimer();
}

installSessionGuard();

// Initialize API instance globally
const api = new ESP32WebAPI();
