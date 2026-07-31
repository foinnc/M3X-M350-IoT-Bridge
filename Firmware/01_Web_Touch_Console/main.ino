#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
// *** 必须先安装 ModbusMaster 库 (D. G. Lunn 版本) ***
#include <ModbusMaster.h> 

// --- 指示灯定义 ---
#define LED_PIN 8  // GPIO8，低电平点亮

const char* ssid = "M3X";
const char* password = "12345678";

// --- 硬件定义 ---
const int RXD1 = 20;
const int TXD1 = 21;

// --- Modbus 设置 ---
const uint8_t SLAVE_ID = 0x01; 
const uint16_t START_REG_WRITE = 6908; 
const uint16_t REG_READ_WORK = 7080; 
const uint16_t REG_READ_MACH = 7260; 
const uint16_t REG_READ_STAT = 10002; 

ModbusMaster node; 
unsigned long lastReadTime = 0;
const long readInterval = 100; 

WebServer server(80);
WebSocketsServer webSocket = WebSocketsServer(81); 

// LED 状态变量
bool led_state = false; 

// 闪烁 LED 函数
void blinkLED() {
    led_state = !led_state;
    digitalWrite(LED_PIN, led_state ? LOW : HIGH);
}

// *********************************************************************************
// ********* HTML 界面 (保持不变) *********
// *********************************************************************************
const char MAIN_page[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1, user-scalable=no, maximum-scale=1, user-scalable=0">
<title>M3X 5-Axis Pro</title>
<style>
* { box-sizing: border-box; }
:root {
    --bg-color: #121212; --card-bg: #1e1e24; --btn-bg: #2a2d3a; --btn-border: #383c4d;
    --text-main: #ffffff; --text-sub: #757985; --text-accent: #7aa5f5;
    --btn-start: #00b894; --btn-pause: #fdcb6e; --btn-reset: #ff7675; 
    --btn-speed: #0984e3; --btn-fkey:  #6c5ce7; 
}
body { 
    font-family: 'Roboto', sans-serif; background-color: var(--bg-color); color: var(--text-main);
    margin: 0; padding: 8px; display: flex; flex-direction: column; align-items: center;
    min-height: 100vh; touch-action: manipulation; -webkit-user-select: none; user-select: none;
}
@keyframes breathing {
    0% { box-shadow: 0 0 2px rgba(0, 230, 118, 0.3); transform: scale(1); }
    50% { box-shadow: 0 0 8px rgba(0, 230, 118, 0.8); transform: scale(1.2); }
    100% { box-shadow: 0 0 2px rgba(0, 230, 118, 0.3); transform: scale(1); }
}
.header-panel {
    width: 100%; max-width: 450px; display: grid; 
    grid-template-columns: 1fr auto 1fr; 
    align-items: center; gap: 5px; margin-bottom: 10px; padding: 5px 2px;
}
.left-group { display: flex; align-items: center; gap: 12px; justify-self: start; }
h2 { margin: 0; font-size: 18px; font-weight: 900; color: var(--text-main); letter-spacing: 1px; line-height: 1; }
.conn-wrapper { display: flex; align-items: center; gap: 6px; }
.dot { width: 8px; height: 8px; border-radius: 50%; background: #555; transition: all 0.3s; }
.dot.ok { background: #00e676; animation: breathing 2s infinite ease-in-out; } 
#conn-text { font-size: 10px; color: #666; font-weight: bold; padding-top: 1px;}
.sys-status {
    justify-self: center; font-size: 14px; font-weight: bold;
    color: #fff; background: #333; padding: 6px 12px; border-radius: 6px;
    min-width: 80px; text-align: center; white-space: nowrap;
    border: 1px solid #444; transition: all 0.3s;
}
.st-idle { background-color: rgba(0, 184, 148, 0.2); color: #00b894; border-color: #00b894; }
.st-run  { background-color: rgba(9, 132, 227, 0.2); color: #74b9ff; border-color: #0984e3; }
.st-reset { background-color: rgba(255, 71, 87, 0.25); color: #ff6b81; border-color: #ff4757; box-shadow: 0 0 8px rgba(255, 71, 87, 0.4); }
.st-off  { background-color: #2d3436; color: #636e72; border-color: #636e72; }
.lock-wrapper { justify-self: end; display: flex; align-items: center; }
.switch { position: relative; display: inline-block; width: 46px; height: 24px; }
.switch input { opacity: 0; width: 0; height: 0; }
.slider { position: absolute; cursor: pointer; top: 0; left: 0; right: 0; bottom: 0; background-color: #444; transition: .4s; border-radius: 34px; border: 1px solid #666; }
.slider:before { position: absolute; content: "🔒"; font-size: 12px; display:flex; justify-content:center; align-items:center; height: 20px; width: 20px; left: 1px; bottom: 1px; background-color: #bbb; transition: .4s; border-radius: 50%; color: #333;}
input:checked + .slider { background-color: var(--btn-start); border-color: var(--btn-start); }
input:checked + .slider:before { transform: translateX(22px); content: "🔓"; background-color: #fff; }
.ui-disabled { opacity: 0.3; pointer-events: none; filter: grayscale(0.8); transition: opacity 0.3s; }
.dro-panel {
    background-color: var(--card-bg); border-radius: 12px; padding: 12px; width: 100%; max-width: 450px;
    box-shadow: 0 4px 20px rgba(0,0,0,0.3); display: grid; grid-template-columns: 30px 1fr 1fr; gap: 6px;
    align-items: center; margin-bottom: 5px;
}
.dro-header { color: var(--text-sub); font-size: 10px; text-align: right; padding-bottom: 5px; border-bottom: 1px solid rgba(255,255,255,0.1); }
.axis-name { font-size: 18px; font-weight: 900; color: var(--text-accent); }
.coord-val { font-family: 'Courier New', monospace; text-align: right; letter-spacing: 0.5px; }
.work-pos { font-size: 22px; font-weight: bold; color: var(--text-main); }
.mach-pos { font-size: 14px; color: #555; }
.controls-wrapper { width: 100%; max-width: 450px; display: flex; flex-direction: column; }
.section-title {
    width: 100%; color: var(--text-sub); font-size: 12px; font-weight: bold; letter-spacing: 1px;
    margin: 12px 0 6px 5px; text-transform: uppercase; display: flex; align-items: center;
}
.section-title::after { content: ''; flex: 1; height: 1px; background: #333; margin-left: 10px; }
button {
    background-color: var(--btn-bg); color: var(--text-main); border: 1px solid var(--btn-border);
    border-radius: 12px; font-weight: 600; cursor: pointer; box-shadow: 0 4px 0 rgba(0,0,0,0.3);
    transition: transform 0.05s, box-shadow 0.05s; -webkit-tap-highlight-color: transparent;
    display: flex; justify-content: center; align-items: center; position: relative; overflow: hidden;
}
button:active, button.active { transform: translateY(3px); box-shadow: 0 1px 0 rgba(0,0,0,0.3); filter: brightness(1.2); }
.sys-grid { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 8px; }
.btn-sys { height: 60px; font-size: 14px; flex-direction: column; gap: 4px; }
.btn-sys i { font-size: 20px; font-style: normal; }
.btn-start { background-color: rgba(0, 184, 148, 0.15); border-color: var(--btn-start); color: var(--btn-start); }
.btn-pause { background-color: rgba(253, 203, 110, 0.15); border-color: var(--btn-pause); color: var(--btn-pause); }
.btn-reset { background-color: rgba(255, 118, 117, 0.15); border-color: var(--btn-reset); color: var(--btn-reset); }
.jog-container {
    background-color: #181818; border-radius: 12px; padding: 10px 5px; border: 1px solid #333;
    display: grid; grid-template-columns: 0.8fr 2.5fr 0.8fr; gap: 8px; align-items: center;
}
.jog-btn-base { height: 60px; width: 100%; font-size: 20px; position: relative; border-radius: 12px; }
.col-ab { display: flex; flex-direction: column; gap: 8px; height: 100%; justify-content: center; }
.btn-ab { height: 50px; font-size: 16px; }
.btn-ab::after { content: attr(data-label); position: absolute; bottom: 2px; right: 3px; font-size: 9px; opacity: 0.6; font-weight: normal; }
.col-z { display: flex; flex-direction: column; gap: 30px; height: 100%; justify-content: center; }
.btn-z-up { height: 70px; font-size: 24px; border-radius: 50% 50% 12px 12px; }
.btn-z-down { height: 70px; font-size: 24px; border-radius: 12px 12px 50% 50%; }
.xy-pad {
    display: grid; grid-template-columns: 1fr 1fr 1fr; grid-template-rows: 1fr 1fr 1fr;
    gap: 5px; width: 100%; aspect-ratio: 1/1;
}
.btn-center { 
    grid-column: 2; grid-row: 2; z-index: 2; border-radius: 50%; height: 100%;
    background-color: rgba(9, 132, 227, 0.15); border-color: var(--btn-speed); color: var(--btn-speed);
    box-shadow: 0 0 10px rgba(0,0,0,0.5); flex-direction: column; font-size: 10px;
}
.speed-icon { font-size: 16px; margin-bottom: -2px; }
.btn-dir { width: 100%; height: 100%; font-size: 24px; }
.btn-y-plus { grid-column: 2; grid-row: 1; border-radius: 50% 50% 10px 10px; }
.btn-y-minus { grid-column: 2; grid-row: 3; border-radius: 10px 10px 50% 50%; }
.btn-x-minus { grid-column: 1; grid-row: 2; border-radius: 50% 10px 10px 50%; }
.btn-x-plus { grid-column: 3; grid-row: 2; border-radius: 10px 50% 50% 10px; }
.fkey-grid { display: grid; grid-template-columns: 1fr 1fr 1fr; gap: 10px; }
.btn-fkey { height: 50px; font-size: 16px; font-weight: bold; color: #ccc; border-color: #555; background: #25252b; }
.btn-fkey:active { background: var(--btn-fkey); color: #fff; border-color: var(--btn-fkey); }
#footer { margin-top: 20px; font-size: 12px; color: var(--text-sub); display: flex; align-items: center; justify-content: center;}
.dot { width: 8px; height: 8px; border-radius: 50%; background: #444; margin-right: 8px; }
.dot.ok { background: #00e676; box-shadow: 0 0 6px #00e676; }
</style>
</head>
<body>
<div class="header-panel">
    <div class="left-group">
        <h2>M3X</h2>
        <div class="conn-wrapper">
            <span id="conn-dot" class="dot"></span>
            <span id="conn-text">Connecting...</span>
        </div>
    </div>
    <div id="sys-state" class="sys-status">OFFLINE</div>
    <div class="lock-wrapper">
        <label class="switch"><input type="checkbox" id="safety-switch" onchange="toggleLock()"><span class="slider"></span></label>
    </div>
</div>
<div class="dro-panel">
    <div class="dro-header" style="text-align:left">AXIS</div><div class="dro-header">WORK (7080)</div><div class="dro-header">MACH (7260)</div>
    <div class="axis-name">X</div><div id="w-X" class="coord-val work-pos">0.000</div><div id="m-X" class="coord-val mach-pos">0.000</div>
    <div class="axis-name">Y</div><div id="w-Y" class="coord-val work-pos">0.000</div><div id="m-Y" class="coord-val mach-pos">0.000</div>
    <div class="axis-name">Z</div><div id="w-Z" class="coord-val work-pos">0.000</div><div id="m-Z" class="coord-val mach-pos">0.000</div>
    <div class="axis-name">A</div><div id="w-A" class="coord-val work-pos">0.000</div><div id="m-A" class="coord-val mach-pos">0.000</div>
    <div class="axis-name">B</div><div id="w-B" class="coord-val work-pos">0.000</div><div id="m-B" class="coord-val mach-pos">0.000</div>
</div>
<div id="ctrl-container" class="controls-wrapper ui-disabled">
    <div class="section-title">System</div>
    <div class="sys-grid">
        <button class="btn-sys btn-start" onclick="sendCmd('START01')"><i>▶</i>START</button>
        <button class="btn-sys btn-pause" onclick="sendCmd('PAUSE01')"><i>⏸</i>PAUSE</button>
        <button class="btn-sys btn-reset" onclick="sendCmd('RESET01')"><i>■</i>RESET</button>
    </div>
    <div class="section-title">Jog Control</div>
    <div class="jog-container">
        <div class="col-ab">
            <button class="jog-btn-base btn-ab" data-axis="A" data-dir="+" data-label="A">A+</button>
            <button class="jog-btn-base btn-ab" data-axis="A" data-dir="-">A-</button>
            <div style="height:5px"></div>
            <button class="jog-btn-base btn-ab" data-axis="B" data-dir="+" data-label="B">B+</button>
            <button class="jog-btn-base btn-ab" data-axis="B" data-dir="-">B-</button>
        </div>
        <div class="xy-pad">
            <button class="jog-btn-base btn-dir btn-y-plus" data-axis="Y" data-dir="+">Y+</button>
            <button class="jog-btn-base btn-dir btn-x-minus" data-axis="X" data-dir="-">X-</button>
            <button class="jog-btn-base btn-center" id="btn-speed"><span id="speed-icon" class="speed-icon">🚀</span><span id="speed-txt" style="font-size:9px">HIGH</span></button>
            <button class="jog-btn-base btn-dir btn-x-plus" data-axis="X" data-dir="+">X+</button>
            <button class="jog-btn-base btn-dir btn-y-minus" data-axis="Y" data-dir="-">Y-</button>
        </div>
        <div class="col-z">
            <button class="jog-btn-base btn-z-up" data-axis="Z" data-dir="+">Z+</button>
            <button class="jog-btn-base btn-z-down" data-axis="Z" data-dir="-">Z-</button>
        </div>
    </div>
    <div class="section-title">K-Keys</div>
    <div class="fkey-grid">
        <button class="btn-fkey" onclick="sendCmd('F101')">K1</button>
        <button class="btn-fkey" onclick="sendCmd('F201')">K2</button>
        <button class="btn-fkey" onclick="sendCmd('F301')">K3</button>
        <button class="btn-fkey" onclick="sendCmd('F401')">K4</button>
        <button class="btn-fkey" onclick="sendCmd('F501')">K5</button>
        <button class="btn-fkey" onclick="sendCmd('F601')">K6</button>
    </div>
</div>
<div id="footer"><span id="conn-dot" class="dot"></span><span id="conn-text">Connecting...</span></div>
<script>
var socket; var pressTimer=null; var activeBtn=null; var isUnlocked=false; var isHighSpeed=true;
const LONG_PRESS_MS=300; 
function toggleLock(){ isUnlocked=document.getElementById('safety-switch').checked; var d=document.getElementById('ctrl-container'); if(isUnlocked) d.classList.remove('ui-disabled'); else { d.classList.add('ui-disabled'); if(activeBtn) endPress(null); } }
document.getElementById('btn-speed').addEventListener('click', function() {
    if(!isUnlocked) return;
    isHighSpeed = !isHighSpeed;
    var t=document.getElementById('speed-txt'), i=document.getElementById('speed-icon');
    if(isHighSpeed) { t.innerText="HIGH"; i.innerText="🚀"; this.style.color="#0984e3"; this.style.borderColor="#0984e3"; }
    else { t.innerText="LOW"; i.innerText="🐢"; this.style.color="#ccc"; this.style.borderColor="#666"; }
    sendCmd(isHighSpeed ? "HF01" : "LF01"); 
});
function updateConn(s,k){ 
    var txt = document.getElementById('conn-text');
    var dot = document.getElementById('conn-dot');
    txt.innerText = s;
    if(k) { dot.classList.add('ok'); txt.style.color = "#00e676"; } else { dot.classList.remove('ok'); txt.style.color = "#777"; }
}
function initWebSocket() {
  socket = new WebSocket("ws://" + location.hostname + ":81/");
  socket.onopen = function() { updateConn("Connected", true); };
  socket.onclose = function() { updateConn("Disconnected", false); setTimeout(initWebSocket, 1000); };
  socket.onerror = function() { updateConn("Error", false); };
  socket.onmessage = function(e) {
      var msg = e.data; 
      if (msg.startsWith("POS:")) {
          var vals = msg.substring(4).split(",");
          if(vals.length >= 11) {
              document.getElementById('w-X').innerText = vals[0]; document.getElementById('m-X').innerText = vals[5];
              document.getElementById('w-Y').innerText = vals[1]; document.getElementById('m-Y').innerText = vals[6];
              document.getElementById('w-Z').innerText = vals[2]; document.getElementById('m-Z').innerText = vals[7];
              document.getElementById('w-A').innerText = vals[3]; document.getElementById('m-A').innerText = vals[8];
              document.getElementById('w-B').innerText = vals[4]; document.getElementById('m-B').innerText = vals[9];
              var s = vals[10];
              var st = document.getElementById('sys-state');
              st.className = "sys-status"; 
              if(s=="0") { st.innerText="IDLE"; st.classList.add("st-idle"); }
              else if(s=="1") { st.innerText="BUSY"; st.classList.add("st-run"); }
              else if(s=="2") { st.innerText="RESET"; st.classList.add("st-reset"); }
              else { st.innerText="CODE:"+s; st.classList.add("st-off"); }
          }
      }
  };
}
function sendCmd(cmd) { if(!isUnlocked) return; if(socket && socket.readyState===WebSocket.OPEN) socket.send(cmd); }
function startPress(e) {
    if(!isUnlocked) return; 
    if(e.cancelable && !e.target.matches('button')) return;
    if(e.cancelable && e.type === 'touchstart') e.preventDefault(); 
    if(activeBtn) return; var btn = e.target.closest('button'); if(!btn) return;
    if(btn.getAttribute('onclick') || btn.id==='btn-speed') return;
    activeBtn = btn; activeBtn.classList.add('active');
    var axis=btn.getAttribute('data-axis'), dir=btn.getAttribute('data-dir');
    if(axis && dir) {
        sendCmd(axis + dir + "01"); 
        pressTimer = setTimeout(function(){ if(activeBtn) sendCmd(axis+dir+"02"); }, LONG_PRESS_MS);
    } else if (btn.innerHTML.startsWith("F") || btn.classList.contains('btn-sys')) {
        if(btn.onclick) return;
        var txt = btn.innerText.trim();
        if(txt.includes("START")) sendCmd("START01");
        else if(txt.includes("PAUSE")) sendCmd("PAUSE01");
        else if(txt.includes("RESET")) sendCmd("RESET01");
        else sendCmd(txt + "01"); 
    }
}
function endPress(e) {
    if(e && e.cancelable && e.type==='touchend') e.preventDefault();
    if(!activeBtn) return; 
    if(pressTimer) { clearTimeout(pressTimer); pressTimer=null; }
    var axis=activeBtn.getAttribute('data-axis'), dir=activeBtn.getAttribute('data-dir');
    if(axis && dir) sendCmd(axis + dir + "00"); 
    activeBtn.classList.remove('active'); activeBtn.blur(); activeBtn=null;
}
window.onload = function() {
    initWebSocket(); toggleLock();
    var btns = document.querySelectorAll('button');
    btns.forEach(function(btn) {
        if(btn.getAttribute('onclick') || btn.id==='btn-speed') return;
        btn.addEventListener('touchstart', startPress, {passive:false}); btn.addEventListener('touchend', endPress, {passive:false});
        btn.addEventListener('mousedown', startPress); btn.addEventListener('mouseup', endPress); btn.addEventListener('mouseleave', endPress); 
    });
};
</script>
</body>
</html>
)HTML";

// =========================================================================
// ========================= C++ 逻辑部分 ==================================
// =========================================================================

// --- 键值映射表 ---
const uint16_t KeyCodeMap(const String& cmd) {
  if (cmd == "X+") return 0x015e; 
  if (cmd == "X-") return 0x015f; 
  if (cmd == "Y+") return 0x0160; 
  if (cmd == "Y-") return 0x0161; 
  if (cmd == "Z+") return 0x0162; 
  if (cmd == "Z-") return 0x0163; 
  if (cmd == "A+") return 0x0164; 
  if (cmd == "A-") return 0x0165; 
  if (cmd == "B+") return 0x0109; 
  if (cmd == "B-") return 0x0166; 

  if (cmd == "START") return 0x0148;
  if (cmd == "PAUSE") return 0x0149;
  if (cmd == "RESET") return 0x0147;

  if (cmd == "HF") return 0x0184; 
  if (cmd == "LF") return 0x0184;

  if (cmd == "F1") return 0x0600;
  if (cmd == "F2") return 0x0601;
  if (cmd == "F3") return 0x0602;
  if (cmd == "F4") return 0x0603;
  if (cmd == "F5") return 0x0604;
  if (cmd == "F6") return 0x0605;

  return 0x0000; 
}

// ------------------------------------------------
// --- 写入 Modbus 0x10 函数 (按键发送) ---
// ------------------------------------------------
void sendModbusCommand(uint16_t keyCode, uint16_t actionState) {
    uint8_t result;
    node.clearTransmitBuffer();
    
    uint32_t combinedInt = ((uint32_t)actionState << 16) | keyCode;
    float floatVal = (float)combinedInt;

    union {
        float f;
        uint32_t i;
    } data;
    data.f = floatVal;

    uint16_t lowWord = (uint16_t)(data.i & 0xFFFF);        
    uint16_t highWord = (uint16_t)((data.i >> 16) & 0xFFFF); 

    node.setTransmitBuffer(0, lowWord); 
    node.setTransmitBuffer(1, highWord);     
    
    result = node.writeMultipleRegisters(START_REG_WRITE, 2); 
    
    // 如果通讯成功，触发闪烁
    if (result == node.ku8MBSuccess) blinkLED();
}

// ------------------------------------------------
// --- 读取 Modbus 0x03 函数 (CDAB 顺序) ---
// ------------------------------------------------
void readCncStatus() {
    uint8_t result;
    String msg = "POS:";
    bool success_flag = false;

    // 1. 读取工件坐标 (10个寄存器)
    result = node.readHoldingRegisters(REG_READ_WORK, 10);
    if (result == node.ku8MBSuccess) {
        success_flag = true;
        for (int i = 0; i < 5; i++) {
            uint16_t r1 = node.getResponseBuffer(i * 2);
            uint16_t r2 = node.getResponseBuffer(i * 2 + 1);
            union { uint32_t i; float f; } data;
            data.i = ((uint32_t)r2 << 16) | r1; 
            msg += String(data.f, 3) + ",";
        }
    } else {
        msg += "ERR,ERR,ERR,ERR,ERR,";
    }

    // 2. 读取机械坐标
    result = node.readHoldingRegisters(REG_READ_MACH, 10);
    if (result == node.ku8MBSuccess) {
        success_flag = true;
        for (int i = 0; i < 5; i++) {
            uint16_t r1 = node.getResponseBuffer(i * 2);
            uint16_t r2 = node.getResponseBuffer(i * 2 + 1);
            union { uint32_t i; float f; } data;
            data.i = ((uint32_t)r2 << 16) | r1; 
            msg += String(data.f, 3) + ",";
        }
    } else {
        msg += "ERR,ERR,ERR,ERR,ERR,";
    }

    // 3. 读取 IDLE 状态
    result = node.readHoldingRegisters(REG_READ_STAT, 2);
    if (result == node.ku8MBSuccess) {
        success_flag = true;
        uint16_t r1 = node.getResponseBuffer(0);
        uint16_t r2 = node.getResponseBuffer(1);
        union { uint32_t i; float f; } data;
        data.i = ((uint32_t)r2 << 16) | r1; 
        msg += String((int)data.f); 
    } else {
        msg += "99";
    }

    // 只要读取成功，就触发闪烁
    if(success_flag) blinkLED();

    webSocket.broadcastTXT(msg);
}

void handleRoot() {
    server.send(200, "text/html; charset=UTF-8", MAIN_page);
}

void onWebSocketEvent(uint8_t client_num, WStype_t type, uint8_t * payload, size_t length) {
  if (type == WStype_TEXT) {
    String cmd = "";
    for(size_t i=0; i<length; i++) cmd += (char)payload[i];
    
    if (length >= 3) {
      String axisPart = cmd.substring(0, length - 2); 
      String codePart = cmd.substring(length - 2);    
      uint16_t keyCode = KeyCodeMap(axisPart);
      uint16_t actionState = codePart.toInt(); 
      if(keyCode != 0) {
          sendModbusCommand(keyCode, actionState);
      }
    }
  }
}

void setup() {
  // 配置指示灯引脚
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW); // 通电后常亮 (低电平点亮)

  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, RXD1, TXD1); 
  node.begin(SLAVE_ID, Serial1); 

  WiFi.softAP(ssid, password);
  Serial.println("AP IP: " + WiFi.softAPIP().toString());

  server.on("/", handleRoot);
  server.begin();

  webSocket.begin();
  webSocket.onEvent(onWebSocketEvent);
}

void loop() {
  server.handleClient();
  webSocket.loop(); 

  if (millis() - lastReadTime >= readInterval) {
    readCncStatus();
    lastReadTime = millis();
  }
}