/**
 * ESP32 USB Port Controller
 * Modern web interface for USB port management
 */

class USBController {
    constructor() {
        this.ports = [];
        this.totalPorts = 10;
        this.refreshInterval = null;
        this.init();
    }

    async init() {
        console.log('Initializing USB Controller...');
        this.setupEventListeners();
        await this.checkWiFiStatus();
        await this.loadUSBStatus();
        this.startAutoRefresh();
        this.updateClock();
    }

    setupEventListeners() {
        // Auto-refresh checkbox if exists
        const autoRefreshCheckbox = document.getElementById('autoRefresh');
        if (autoRefreshCheckbox) {
            autoRefreshCheckbox.addEventListener('change', (e) => {
                if (e.target.checked) {
                    this.startAutoRefresh();
                } else {
                    this.stopAutoRefresh();
                }
            });
        }
    }

    async checkWiFiStatus() {
        const wifiStatus = document.getElementById('wifiStatus');
        const wifiText = document.getElementById('wifiText');

        try {
            const response = await fetch('/api/wifi/status');
            const data = await response.json();

            if (data.connected) {
                wifiStatus.querySelector('.status-indicator').className = 'status-indicator status-connected';
                wifiText.textContent = `Connected to ${data.ssid}`;
            } else {
                wifiStatus.querySelector('.status-indicator').className = 'status-indicator status-disconnected';
                wifiText.textContent = 'WiFi disconnected';
            }
        } catch (error) {
            console.error('Error checking WiFi status:', error);
            wifiStatus.querySelector('.status-indicator').className = 'status-indicator status-disconnected';
            wifiText.textContent = 'WiFi status unknown';
        }
    }

    async loadUSBStatus() {
        const loadingMessage = document.getElementById('loadingMessage');
        const usbPortsGrid = document.getElementById('usbPortsGrid');

        try {
            loadingMessage.style.display = 'block';
            usbPortsGrid.style.display = 'none';

            const response = await fetch('/api/usb/status');
            const data = await response.json();

            if (data.success) {
                this.ports = data.ports || this.generateDummyPorts();
                this.renderUSBPorts();
                this.updateStatistics();

                loadingMessage.style.display = 'none';
                usbPortsGrid.style.display = 'grid';
            } else {
                throw new Error(data.message || 'Failed to load USB status');
            }
        } catch (error) {
            console.error('Error loading USB status:', error);
            // Show dummy data for demonstration
            this.ports = this.generateDummyPorts();
            this.renderUSBPorts();
            this.updateStatistics();

            loadingMessage.style.display = 'none';
            usbPortsGrid.style.display = 'grid';

            this.showMessage('Unable to connect to device. Showing demo data.', 'warning');
        }
    }

    generateDummyPorts() {
        const dummyPorts = [];
        for (let i = 1; i <= this.totalPorts; i++) {
            dummyPorts.push({
                id: i,
                name: `USB Port ${i}`,
                status: Math.random() > 0.5 ? 'active' : 'inactive',
                lastToggled: new Date(Date.now() - Math.random() * 86400000).toISOString()
            });
        }
        return dummyPorts;
    }

    renderUSBPorts() {
        const usbPortsGrid = document.getElementById('usbPortsGrid');
        usbPortsGrid.innerHTML = '';

        this.ports.forEach(port => {
            const portCard = this.createPortCard(port);
            usbPortsGrid.appendChild(portCard);
        });
    }

    createPortCard(port) {
        const card = document.createElement('div');
        card.className = `usb-port-card ${port.status} fade-in`;
        card.id = `port-${port.id}`;

        const isActive = port.status === 'active';
        const buttonText = isActive ? 'Unplug' : 'Plug';
        const buttonClass = isActive ? 'unplug-btn' : 'plug-btn';
        const statusText = isActive ? 'Plugged' : 'Unplugged';
        const statusIcon = isActive ? 'ACTIVE' : 'INACTIVE';

        card.innerHTML = `
            <div class="port-header">
                <div class="port-number">Port ${port.id}</div>
                <div class="port-status ${port.status}">
                    ${statusText}
                </div>
            </div>
            <button class="port-toggle-btn ${buttonClass}" onclick="usbController.togglePort(${port.id}, ${!isActive})">
                ${buttonText}
            </button>
            <div style="margin-top: 12px; font-size: 0.85em; color: #666;">
                Last updated: ${this.formatDateTime(port.lastToggled)}
            </div>
        `;

        return card;
    }

    async togglePort(portId, newState) {
        const portCard = document.getElementById(`port-${portId}`);
        const button = portCard.querySelector('.port-toggle-btn');

        // Show loading state
        button.disabled = true;
        button.textContent = 'Processing...';
        portCard.className = portCard.className.replace(/\b(active|inactive)\b/g, 'loading');

        try {
            const response = await fetch('/api/usb/toggle', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({
                    portId: portId,
                    state: newState
                })
            });

            const result = await response.json();

            if (result.success) {
                // Update local port data
                const port = this.ports.find(p => p.id === portId);
                if (port) {
                    port.status = newState ? 'active' : 'inactive';
                    port.lastToggled = new Date().toISOString();
                }

                // Re-render the specific port
                const newCard = this.createPortCard(port);
                portCard.parentNode.replaceChild(newCard, portCard);

                // Update statistics
                this.updateStatistics();

                // Show success message
                this.showMessage(
                    `Port ${portId} has been ${newState ? 'plugged' : 'unplugged'} successfully!`,
                    'success'
                );
            } else {
                throw new Error(result.message || 'Toggle operation failed');
            }
        } catch (error) {
            console.error('Error toggling port:', error);

            // Reset button state
            button.disabled = false;
            const isCurrentlyActive = this.ports.find(p => p.id === portId)?.status === 'active';
            button.textContent = isCurrentlyActive ? 'Unplug' : 'Plug';
            button.className = `port-toggle-btn ${isCurrentlyActive ? 'unplug-btn' : 'plug-btn'}`;
            portCard.className = portCard.className.replace('loading', isCurrentlyActive ? 'active' : 'inactive');

            this.showMessage(`Failed to toggle port ${portId}. Please try again.`, 'error');
        }
    }

    async toggleAllPorts(state) {
        const message = state ? 'plugging all ports' : 'unplugging all ports';
        this.showMessage(`${message.charAt(0).toUpperCase() + message.slice(1)}...`, 'info');

        const promises = this.ports.map(port => {
            if ((state && port.status === 'inactive') || (!state && port.status === 'active')) {
                return this.togglePort(port.id, state);
            }
            return Promise.resolve();
        });

        try {
            await Promise.all(promises);
            this.showMessage(
                `All ports have been ${state ? 'plugged' : 'unplugged'} successfully!`,
                'success'
            );
        } catch (error) {
            this.showMessage('Some ports failed to toggle. Please check individual ports.', 'warning');
        }
    }

    updateStatistics() {
        const activePorts = this.ports.filter(p => p.status === 'active').length;
        const inactivePorts = this.ports.filter(p => p.status === 'inactive').length;
        const lastUpdate = new Date().toLocaleTimeString();

        document.getElementById('totalPorts').textContent = this.totalPorts;
        document.getElementById('activePorts').textContent = activePorts;
        document.getElementById('inactivePorts').textContent = inactivePorts;
        document.getElementById('lastUpdate').textContent = lastUpdate;
    }

    async refreshStatus() {
        await this.loadUSBStatus();
        this.showMessage('Status refreshed successfully!', 'success');
    }

    startAutoRefresh() {
        this.stopAutoRefresh(); // Clear any existing interval
        this.refreshInterval = setInterval(() => {
            this.loadUSBStatus();
        }, 30000); // Refresh every 30 seconds
    }

    stopAutoRefresh() {
        if (this.refreshInterval) {
            clearInterval(this.refreshInterval);
            this.refreshInterval = null;
        }
    }

    updateClock() {
        const deviceTime = document.getElementById('deviceTime');
        if (deviceTime) {
            deviceTime.textContent = new Date().toLocaleTimeString();
        }
        setTimeout(() => this.updateClock(), 1000);
    }

    formatDateTime(dateString) {
        const date = new Date(dateString);
        return date.toLocaleString();
    }

    showMessage(text, type = 'info') {
        const messageArea = document.getElementById('messageArea');
        if (!messageArea) return;

        messageArea.textContent = text;
        messageArea.className = `message-area message-${type}`;
        messageArea.style.display = 'block';

        // Auto-hide after 5 seconds for success messages
        if (type === 'success') {
            setTimeout(() => {
                messageArea.style.display = 'none';
            }, 5000);
        }
    }
}

// Global functions for HTML onclick handlers
function toggleAllPorts(state) {
    if (window.usbController) {
        window.usbController.toggleAllPorts(state);
    }
}

function refreshStatus() {
    if (window.usbController) {
        window.usbController.refreshStatus();
    }
}

// WiFi Configuration specific functions
class WiFiConfig {
    constructor() {
        this.selectedNetwork = null;
        this.init();
    }

    init() {
        if (document.getElementById('wifiForm')) {
            this.setupWiFiEventListeners();
            this.loadWiFiStatus();
        }
    }

    setupWiFiEventListeners() {
        const wifiForm = document.getElementById('wifiForm');
        if (wifiForm) {
            wifiForm.addEventListener('submit', this.handleWiFiSubmit.bind(this));
        }
    }

    async loadWiFiStatus() {
        try {
            const response = await fetch('/api/wifi/status');
            const data = await response.json();

            const statusDiv = document.getElementById('status');
            if (!statusDiv) return;

            if (data.connected) {
                statusDiv.innerHTML = `
                    <strong>Connected to WiFi</strong><br>
                    <strong>Network:</strong> ${data.ssid}<br>
                    <strong>IP Address:</strong> ${data.ip}<br>
                    <strong>Signal Strength:</strong> ${data.rssi} dBm
                `;
                statusDiv.className = 'status-card connected';
            } else {
                statusDiv.innerHTML = `
                    <strong>Not Connected</strong><br>
                    Status: ${data.status || 'Disconnected'}
                `;
                statusDiv.className = 'status-card disconnected';
            }
        } catch (error) {
            console.error('Error loading WiFi status:', error);
            const statusDiv = document.getElementById('status');
            if (statusDiv) {
                statusDiv.innerHTML = `
                    <strong>Error</strong><br>
                    Could not load WiFi status
                `;
            }
        }
    }

    async scanNetworks() {
        const networksDiv = document.getElementById('networks');
        if (!networksDiv) return;

        networksDiv.innerHTML = '<div class="loading">Scanning for networks...</div>';

        try {
            const response = await fetch('/api/wifi/scan');
            const data = await response.json();

            if (data.count === 0) {
                networksDiv.innerHTML = '<div class="loading">No networks found</div>';
                return;
            }

            let html = '<h3>Available Networks:</h3>';
            data.networks.forEach((network, index) => {
                const strength = this.getSignalStrength(network.rssi);

                html += `<div class="network-item" onclick="wifiConfig.selectNetwork('${network.ssid}', ${index})">
                    <span>${network.ssid}</span>
                    <div class="signal-strength">${strength} ${network.rssi} dBm</div>
                </div>`;
            });

            networksDiv.innerHTML = html;
        } catch (error) {
            console.error('Error scanning networks:', error);
            networksDiv.innerHTML = '<div class="loading">Scan failed. Please try again.</div>';
        }
    }

    getSignalStrength(rssi) {
        if (rssi > -50) return 'Excellent';
        if (rssi > -60) return 'Good';
        if (rssi > -70) return 'Fair';
        if (rssi > -80) return 'Weak';
        return 'Very Weak';
    }

    selectNetwork(ssid, index) {
        // Remove previous selection
        document.querySelectorAll('.network-item').forEach(item => {
            item.classList.remove('selected');
        });

        // Add selection to clicked item
        const networkItems = document.querySelectorAll('.network-item');
        if (networkItems[index]) {
            networkItems[index].classList.add('selected');
        }

        // Set SSID in form
        const ssidInput = document.getElementById('ssid');
        if (ssidInput) {
            ssidInput.value = ssid;
        }
        this.selectedNetwork = ssid;
    }

    async handleWiFiSubmit(e) {
        e.preventDefault();

        const formData = new FormData(e.target);
        const ssid = formData.get('ssid');
        const password = formData.get('password');

        if (!ssid) {
            this.showWiFiMessage('Please enter a network name (SSID)', 'error');
            return;
        }

        this.showWiFiMessage('Connecting to network...', 'info');

        try {
            const response = await fetch('/api/wifi/connect', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({ ssid, password })
            });

            const result = await response.json();

            if (result.success) {
                this.showWiFiMessage('Successfully connected to ' + ssid + '!', 'success');
                setTimeout(() => this.loadWiFiStatus(), 2000);
            } else {
                this.showWiFiMessage('Failed to connect: ' + (result.message || 'Unknown error'), 'error');
            }
        } catch (error) {
            console.error('Error connecting to WiFi:', error);
            this.showWiFiMessage('Connection failed. Please try again.', 'error');
        }
    }

    showWiFiMessage(text, type) {
        const messageDiv = document.getElementById('message');
        if (!messageDiv) return;

        messageDiv.textContent = text;
        messageDiv.className = type;
        messageDiv.style.display = 'block';

        if (type === 'success') {
            setTimeout(() => {
                messageDiv.style.display = 'none';
            }, 5000);
        }
    }
}

// Global WiFi functions
function scanNetworks() {
    if (window.wifiConfig) {
        window.wifiConfig.scanNetworks();
    }
}

// Initialize appropriate controller based on page
document.addEventListener('DOMContentLoaded', function() {
    if (document.getElementById('usbPortsGrid')) {
        // Main USB controller page
        window.usbController = new USBController();
    } else if (document.getElementById('wifiForm')) {
        // WiFi configuration page
        window.wifiConfig = new WiFiConfig();
    }
});

// Export for potential external use
window.USBController = USBController;
window.WiFiConfig = WiFiConfig;