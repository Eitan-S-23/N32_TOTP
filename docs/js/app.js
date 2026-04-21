/* ------------------------------------------------------------------
 * Web Bluetooth provisioning for the N32-TOTP peripheral.
 *
 * Wire format (mirrors user/app_profile/app_totp_ble.h):
 *   0x01 SET_KEY  [len][key bytes]
 *   0x02 SET_TIME [8 bytes BE uint64 Unix UTC seconds]
 *   0x03 SET_BOTH [len][key bytes][8 bytes BE UTC time]
 *   0x04 SET_TZ   [4 bytes BE int32 seconds east of UTC] (+28800 for UTC+8)
 *
 * UUIDs (derived from the 128-bit UUIDs declared in app_totp_ble.c):
 *   service : 0000278f-08c2-11e1-9073-0e8ac7b41001
 *   write   : 0000278f-08c2-11e1-9073-0e8ac7b40001
 *   notify  : 0000278f-08c2-11e1-9073-0e8ac7b40002
 * ------------------------------------------------------------------ */

const TOTP_SERVICE_UUID = '0000278f-08c2-11e1-9073-0e8ac7b41001';
const TOTP_WRITE_UUID   = '0000278f-08c2-11e1-9073-0e8ac7b40001';
const TOTP_NOTIFY_UUID  = '0000278f-08c2-11e1-9073-0e8ac7b40002';

/* MCU device name prefix — matches CUSTOM_DEVICE_NAME in app_user_config.h. */
const DEVICE_NAME_PREFIX = 'N32-TOTP';

/* UTC+8 (Beijing). Adjust here if you want the on-screen clock in another TZ.
 * The peripheral's TOTP math always uses UTC — this value only shifts the
 * HH:MM:SS label on the LCD. */
const DEVICE_TZ_OFFSET_SECONDS = 8 * 3600;

function getCurrentSeconds() {
  return Math.round(new Date().getTime() / 1000.0);
}

function stripSpaces(str) {
  return str.replace(/\s/g, '');
}

function truncateTo(str, digits) {
  if (str.length <= digits) {
    return str;
  }
  return str.slice(-digits);
}

function parseURLSearch(search) {
  const queryParams = search.substr(1).split('&').reduce(function (q, query) {
    const chunks = query.split('=');
    const key = chunks[0];
    let value = decodeURIComponent(chunks[1]);
    value = isNaN(Number(value)) ? value : Number(value);
    return (q[key] = value, q);
  }, {});

  return queryParams;
}

// RFC 3986 encoding for path/query segments.
function encodeSeg(str) {
  return encodeURIComponent(str).replace(/'/g, '%27');
}

function parseOtpauthUri(uri) {
  const m = /^otpauth:\/\/totp\/([^?]*)(\?.*)?$/i.exec(uri);
  if (!m) throw new Error('Not a TOTP URI');
  const label = decodeURIComponent(m[1] || '');
  const params = new URLSearchParams(m[2] || '');
  const secret = params.get('secret');
  if (!secret) throw new Error('URI missing secret');

  let issuer = params.get('issuer') || '';
  let account = label;
  const colon = label.indexOf(':');
  if (colon >= 0) {
    if (!issuer) issuer = label.slice(0, colon);
    account = label.slice(colon + 1).replace(/^\s+/, '');
  }
  return {
    secret: secret.replace(/\s/g, ''),
    issuer,
    account,
    algorithm: (params.get('algorithm') || 'SHA1').toUpperCase(),
    digits: Number(params.get('digits')) || 6,
    period: Number(params.get('period')) || 30,
  };
}

/* ----- Base32 → raw bytes (shared with otpauth, kept local so the BLE
 * path doesn't depend on internal library shape). ---------------------- */
function base32ToBytes(b32) {
  const cleaned = b32.replace(/=+$/, '').replace(/\s+/g, '').toUpperCase();
  if (!cleaned.length) return new Uint8Array(0);
  const alphabet = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ234567';
  const out = [];
  let bits = 0;
  let value = 0;
  for (let i = 0; i < cleaned.length; i++) {
    const ch = cleaned.charAt(i);
    const idx = alphabet.indexOf(ch);
    if (idx < 0) {
      throw new Error('Invalid Base32 character: ' + ch);
    }
    value = (value << 5) | idx;
    bits += 5;
    if (bits >= 8) {
      bits -= 8;
      out.push((value >>> bits) & 0xFF);
    }
  }
  return new Uint8Array(out);
}

/* Pack a uint64 Unix timestamp into 8 big-endian bytes. JS numbers are
 * safe up to 2^53, which covers dates well past the year 285,000, so plain
 * arithmetic is fine here — no BigInt needed. */
function packUint64BE(seconds) {
  const bytes = new Uint8Array(8);
  let lo = seconds % 0x100000000;
  let hi = Math.floor(seconds / 0x100000000);
  for (let i = 3; i >= 0; i--) {
    bytes[i]     = hi & 0xFF; hi >>>= 8;
    bytes[4 + i] = lo & 0xFF; lo >>>= 8;
  }
  return bytes;
}

function packInt32BE(n) {
  const bytes = new Uint8Array(4);
  const v = (n | 0);          /* coerce to signed 32-bit */
  bytes[0] = (v >>> 24) & 0xFF;
  bytes[1] = (v >>> 16) & 0xFF;
  bytes[2] = (v >>>  8) & 0xFF;
  bytes[3] =  v         & 0xFF;
  return bytes;
}

function toHex(bytes) {
  let s = '';
  for (let i = 0; i < bytes.length; i++) {
    s += bytes[i].toString(16).padStart(2, '0');
  }
  return s;
}

const app = Vue.createApp({
  data() {
    return {
      secret_key: 'JBSWY3DPEHPK3PXP',
      issuer: 'TOTP Generator',
      account: 'user@example.com',
      digits: 6,
      period: 30,
      algorithm: 'SHA1',
      updatingIn: 30,
      token: null,
      clipboardButton: null,
      qrError: '',
      scanning: false,
      scanMsg: '',
      scanError: false,
      scanOk: false,
      importFlash: false,

      /* ----- BLE provisioning state ----- */
      bleSupported: typeof navigator !== 'undefined'
                 && !!navigator.bluetooth,
      bleStatus: 'idle',      /* idle | scanning | connecting | connected | error */
      bleDeviceName: '',
      bleError: '',
      bleMessage: '',
      sendKeyOnConnect: false,
      _bleDevice: null,
      _bleServer: null,
      _bleWriteChar: null,
      _bleNotifyChar: null,
      _bleDisconnectHandler: null,
    };
  },

  mounted: function () {
    this.getKeyFromUrl();
    this.getQueryParameters();
    this.update();

    this.intervalHandle = setInterval(this.update, 1000);

    this.clipboardButton = new ClipboardJS('#clipboard-button');

    this.renderQRCode();
  },

  unmounted: function () {
    clearInterval(this.intervalHandle);
    this.closeScanner();
    this._bleCleanup(true);
  },

  computed: {
    totp: function () {
      return new OTPAuth.TOTP({
        algorithm: this.algorithm,
        digits: this.digits,
        period: this.period,
        secret: OTPAuth.Secret.fromBase32(stripSpaces(this.secret_key)),
      });
    },

    otpauthUri: function () {
      const issuer = (this.issuer || '').trim();
      const account = (this.account || '').trim() || 'user';
      const label = issuer ? `${encodeSeg(issuer)}:${encodeSeg(account)}`
                           : encodeSeg(account);
      const params = new URLSearchParams();
      params.set('secret', stripSpaces(this.secret_key));
      if (issuer) params.set('issuer', issuer);
      params.set('algorithm', this.algorithm);
      params.set('digits', String(this.digits));
      params.set('period', String(this.period));
      return `otpauth://totp/${label}?${params.toString()}`;
    },

    bleConnected: function () {
      return this.bleStatus === 'connected';
    },

    bleBusy: function () {
      return this.bleStatus === 'scanning' || this.bleStatus === 'connecting';
    },

    bleStatusText: function () {
      switch (this.bleStatus) {
        case 'scanning':   return 'Scanning…';
        case 'connecting': return 'Connecting…';
        case 'connected':  return 'Connected';
        case 'error':      return 'Error';
        default:           return 'Disconnected';
      }
    },
  },

  watch: {
    otpauthUri: function () {
      this.renderQRCode();
    },
  },

  methods: {
    update: function () {
      try {
        this.updatingIn = this.period - (getCurrentSeconds() % this.period);
        this.token = truncateTo(this.totp.generate(), this.digits);
      } catch (e) {
        this.token = '------';
      }
    },

    renderQRCode: function () {
      const container = document.getElementById('qrcode');
      if (!container) return;
      try {
        // Auto type, error-correction level M — good for phone scans.
        const qr = qrcode(0, 'M');
        qr.addData(this.otpauthUri);
        qr.make();
        container.innerHTML = qr.createSvgTag({ cellSize: 5, margin: 2 });
        this.qrError = '';
      } catch (e) {
        container.innerHTML = '';
        this.qrError = 'Cannot render QR code: ' + (e && e.message ? e.message : e);
      }
    },

    openScanner: async function () {
      this.scanning = true;
      this.scanError = false;
      this.scanOk = false;
      this.scanMsg = 'Requesting camera…';
      await this.$nextTick();

      if (!navigator.mediaDevices || !navigator.mediaDevices.getUserMedia) {
        this.scanError = true;
        this.scanMsg = 'Camera API unavailable. Use "Upload image" instead.';
        return;
      }

      try {
        this._stream = await navigator.mediaDevices.getUserMedia({
          video: { facingMode: 'environment' },
          audio: false,
        });
      } catch (e) {
        this.scanError = true;
        this.scanMsg = 'Camera blocked: ' + (e && e.message ? e.message : e) +
                       '. You can still upload an image.';
        return;
      }

      const video = this.$refs.video;
      if (!video) return;
      video.srcObject = this._stream;
      try { await video.play(); } catch (_) { /* autoplay may defer */ }
      this.scanMsg = 'Align the QR inside the frame…';
      this._scanCanvas = document.createElement('canvas');
      this._scanLoop();
    },

    _scanLoop: function () {
      if (!this.scanning) return;
      const video = this.$refs.video;
      const canvas = this._scanCanvas;
      if (video && video.readyState === video.HAVE_ENOUGH_DATA && canvas) {
        const w = video.videoWidth, h = video.videoHeight;
        if (w && h) {
          canvas.width = w;
          canvas.height = h;
          const ctx = canvas.getContext('2d', { willReadFrequently: true });
          ctx.drawImage(video, 0, 0, w, h);
          const img = ctx.getImageData(0, 0, w, h);
          const code = jsQR(img.data, w, h, { inversionAttempts: 'dontInvert' });
          if (code && code.data) {
            this._onScanResult(code.data);
            return;
          }
        }
      }
      this._raf = requestAnimationFrame(this._scanLoop);
    },

    _onScanResult: function (text) {
      try {
        this.applyOtpauth(text);
        this.scanError = false;
        this.scanOk = true;
        this.scanMsg = '✓ QR decoded — importing…';
        this._flashImport();
        setTimeout(() => this.closeScanner(), 700);
      } catch (e) {
        this.scanError = true;
        this.scanOk = false;
        this.scanMsg = 'Not a TOTP QR (' + e.message + '). Still scanning…';
        this._raf = requestAnimationFrame(this._scanLoop);
      }
    },

    _flashImport: function () {
      this.importFlash = true;
      if (this._flashTimer) clearTimeout(this._flashTimer);
      this._flashTimer = setTimeout(() => { this.importFlash = false; }, 2500);
    },

    onImageFile: function (ev) {
      const file = ev.target.files && ev.target.files[0];
      ev.target.value = '';
      if (!file) return;
      const img = new Image();
      img.onload = () => {
        const canvas = document.createElement('canvas');
        canvas.width = img.naturalWidth;
        canvas.height = img.naturalHeight;
        const ctx = canvas.getContext('2d');
        ctx.drawImage(img, 0, 0);
        const data = ctx.getImageData(0, 0, canvas.width, canvas.height);
        const code = jsQR(data.data, canvas.width, canvas.height);
        URL.revokeObjectURL(img.src);
        if (!code || !code.data) {
          this.scanError = true;
          this.scanMsg = 'No QR code found in the image.';
          return;
        }
        try {
          this.applyOtpauth(code.data);
          this.scanError = false;
          this.scanOk = true;
          this.scanMsg = '✓ QR decoded — importing…';
          this._flashImport();
          setTimeout(() => this.closeScanner(), 700);
        } catch (e) {
          this.scanError = true;
          this.scanMsg = e.message;
        }
      };
      img.onerror = () => {
        this.scanError = true;
        this.scanMsg = 'Could not load image.';
      };
      img.src = URL.createObjectURL(file);
    },

    applyOtpauth: function (uri) {
      const info = parseOtpauthUri(uri);
      this.secret_key = info.secret;
      this.issuer = info.issuer || '';
      this.account = info.account || '';
      this.algorithm = info.algorithm;
      this.digits = info.digits;
      this.period = info.period;
      this.update();
    },

    closeScanner: function () {
      this.scanning = false;
      if (this._raf) { cancelAnimationFrame(this._raf); this._raf = null; }
      if (this._stream) {
        this._stream.getTracks().forEach(t => t.stop());
        this._stream = null;
      }
    },

    getKeyFromUrl: function () {
      const key = document.location.hash.replace(/[#\/]+/, '');

      if (key.length > 0) {
        this.secret_key = key;
      }
    },
    getQueryParameters: function () {
      const queryParams = parseURLSearch(window.location.search);

      if (queryParams.key) {
        this.secret_key = queryParams.key;
      }

      if (queryParams.digits) {
        this.digits = queryParams.digits;
      }

      if (queryParams.period) {
        this.period = queryParams.period;
      }

      if (queryParams.algorithm) {
        this.algorithm = queryParams.algorithm;
      }

      if (queryParams.issuer) {
        this.issuer = queryParams.issuer;
      }

      if (queryParams.account) {
        this.account = queryParams.account;
      }
    },

    /* ================= BLE provisioning ================= */

    bleConnect: async function () {
      if (!this.bleSupported) {
        this.bleStatus = 'error';
        this.bleError = 'Web Bluetooth is not available in this browser. ' +
                        'Use desktop Chrome/Edge or Chrome on Android.';
        return;
      }

      this.bleError = '';
      this.bleMessage = '';
      this.bleStatus = 'scanning';

      let device;
      try {
        device = await navigator.bluetooth.requestDevice({
          filters: [{ namePrefix: DEVICE_NAME_PREFIX }],
          optionalServices: [TOTP_SERVICE_UUID],
        });
      } catch (e) {
        /* User cancelling the chooser throws a NotFoundError — not an
         * actual error from the user's perspective. */
        this.bleStatus = 'idle';
        if (e && e.name !== 'NotFoundError') {
          this.bleError = 'Scan failed: ' + (e.message || e);
          this.bleStatus = 'error';
        }
        return;
      }

      this._bleDevice = device;
      this.bleDeviceName = device.name || '(unnamed)';
      this.bleStatus = 'connecting';

      this._bleDisconnectHandler = () => this._onBleDisconnected();
      device.addEventListener('gattserverdisconnected', this._bleDisconnectHandler);

      try {
        const server  = await device.gatt.connect();
        const service = await server.getPrimaryService(TOTP_SERVICE_UUID);
        const wrChar  = await service.getCharacteristic(TOTP_WRITE_UUID);

        let ntfChar = null;
        try {
          ntfChar = await service.getCharacteristic(TOTP_NOTIFY_UUID);
          await ntfChar.startNotifications();
          ntfChar.addEventListener('characteristicvaluechanged',
                                   (ev) => this._onBleNotify(ev));
        } catch (_) {
          /* Notifications are optional for provisioning. */
        }

        this._bleServer     = server;
        this._bleWriteChar  = wrChar;
        this._bleNotifyChar = ntfChar;
        this.bleStatus      = 'connected';

        await this._provisionOnConnect();
      } catch (e) {
        this.bleError = 'Connection failed: ' + (e.message || e);
        this.bleStatus = 'error';
        this._bleCleanup(true);
      }
    },

    bleDisconnect: function () {
      this._bleCleanup(true);
      this.bleStatus = 'idle';
      this.bleMessage = 'Disconnected.';
    },

    sendKeyToDevice: async function () {
      if (!this.bleConnected) {
        this.bleError = 'Not connected.';
        return;
      }
      try {
        const keyBytes = base32ToBytes(stripSpaces(this.secret_key));
        if (!keyBytes.length) {
          throw new Error('Secret is empty.');
        }
        if (keyBytes.length > 64) {
          throw new Error('Secret is longer than 64 bytes (TOTP_KEY_MAX).');
        }
        await this._writeCmd(this._buildSetKey(keyBytes));
        this.bleError = '';
        this.bleMessage = `Sent key (${keyBytes.length} B): ${toHex(keyBytes)}`;
      } catch (e) {
        this.bleError = 'Send key failed: ' + (e.message || e);
      }
    },

    sendTimeToDevice: async function () {
      if (!this.bleConnected) {
        this.bleError = 'Not connected.';
        return;
      }
      try {
        await this._writeCmd(this._buildSetTz(DEVICE_TZ_OFFSET_SECONDS));
        await this._writeCmd(this._buildSetTime(getCurrentSeconds()));
        this.bleError = '';
        this.bleMessage = 'Re-synced UTC time + UTC+8 offset.';
      } catch (e) {
        this.bleError = 'Send time failed: ' + (e.message || e);
      }
    },

    _provisionOnConnect: async function () {
      const steps = [];
      try {
        /* Space the writes out so the MCU's UART log has a clear gap
         * between each step — easier to see where a crash occurs. 150 ms
         * is plenty for the BLE stack to settle in between. */
        await this._sleep(200);

        await this._writeCmd(this._buildSetTz(DEVICE_TZ_OFFSET_SECONDS));
        steps.push('TZ=+08:00');
        await this._sleep(150);

        await this._writeCmd(this._buildSetTime(getCurrentSeconds()));
        steps.push('time');
        await this._sleep(150);

        if (this.sendKeyOnConnect) {
          const keyBytes = base32ToBytes(stripSpaces(this.secret_key));
          if (!keyBytes.length) {
            throw new Error('Secret is empty.');
          }
          if (keyBytes.length > 64) {
            throw new Error('Secret is longer than 64 bytes.');
          }
          await this._writeCmd(this._buildSetKey(keyBytes));
          steps.push(`key (${keyBytes.length} B)`);
        }
        this.bleMessage = 'Sent ' + steps.join(', ') + '.';
      } catch (e) {
        this.bleError = 'Provision failed after ' + (steps.length
          ? steps.join(', ') + ': '
          : '') + (e.message || e);
      }
    },

    _sleep: function (ms) {
      return new Promise(resolve => setTimeout(resolve, ms));
    },

    _buildSetTz: function (offsetSeconds) {
      /* [0x04][4-byte BE int32] */
      const payload = packInt32BE(offsetSeconds);
      const out = new Uint8Array(1 + payload.length);
      out[0] = 0x04;
      out.set(payload, 1);
      return out;
    },

    _buildSetTime: function (unixSecondsUtc) {
      /* [0x02][8-byte BE uint64] */
      const payload = packUint64BE(unixSecondsUtc);
      const out = new Uint8Array(1 + payload.length);
      out[0] = 0x02;
      out.set(payload, 1);
      return out;
    },

    _buildSetKey: function (keyBytes) {
      /* [0x01][len][key bytes] */
      const out = new Uint8Array(2 + keyBytes.length);
      out[0] = 0x01;
      out[1] = keyBytes.length & 0xFF;
      out.set(keyBytes, 2);
      return out;
    },

    _writeCmd: async function (bytes) {
      if (!this._bleWriteChar) {
        throw new Error('Write characteristic not ready.');
      }
      /* Prefer write-with-response so the peripheral ACKs each PDU. Some
       * older Chrome builds only expose writeValue(), so we fall back. */
      if (typeof this._bleWriteChar.writeValueWithResponse === 'function') {
        await this._bleWriteChar.writeValueWithResponse(bytes);
      } else {
        await this._bleWriteChar.writeValue(bytes);
      }
    },

    _onBleNotify: function (ev) {
      const value = ev && ev.target && ev.target.value;
      if (!value) return;
      const bytes = new Uint8Array(value.buffer || value);
      this.bleMessage = 'MCU notify: ' + toHex(bytes);
    },

    _onBleDisconnected: function () {
      const wasConnected = this.bleStatus === 'connected';
      this._bleCleanup(false);
      this.bleStatus = 'idle';
      if (wasConnected) {
        this.bleMessage = 'Device disconnected.';
      }
    },

    _bleCleanup: function (disconnect) {
      if (this._bleDevice && this._bleDisconnectHandler) {
        this._bleDevice.removeEventListener('gattserverdisconnected',
                                             this._bleDisconnectHandler);
      }
      if (disconnect && this._bleServer && this._bleServer.connected) {
        try { this._bleServer.disconnect(); } catch (_) {}
      }
      this._bleDevice            = null;
      this._bleServer            = null;
      this._bleWriteChar         = null;
      this._bleNotifyChar        = null;
      this._bleDisconnectHandler = null;
      this.bleDeviceName         = '';
    },
  }
});

app.mount('#app');
