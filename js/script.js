// Init Animation Library
AOS.init({ duration: 800, once: true });

// --- VIBE TOGGLE LOGIC ---
const themeBtn = document.getElementById('theme-toggle');
if (themeBtn) {
    themeBtn.addEventListener('click', () => {
        // Toggle the class on the body to switch between Engineer and Artist modes
        document.body.classList.toggle('artist-mode');
        
        // Update the icon based on the active mode
        const isArtist = document.body.classList.contains('artist-mode');
        themeBtn.innerHTML = isArtist ? '<i class="fas fa-microchip"></i>' : '<i class="fas fa-mask"></i>';
    });
}

// --- IOT TELEMETRY SIMULATOR ---
function updateIOT() {
    // Simulates slight temperature fluctuations
    const temp = (27 + Math.random() * 3).toFixed(1);
    // Gets current system time
    const uptime = new Date().toTimeString().split(' ')[0];
    
    const tempElement = document.getElementById('live-temp');
    const uptimeElement = document.getElementById('uptime');
    
    if (tempElement) tempElement.innerText = temp;
    if (uptimeElement) uptimeElement.innerText = uptime;
}
// Update telemetry every second
setInterval(updateIOT, 1000);

// --- TYPEWRITER EFFECT (ORIGINAL) ---
const phrases = ["Mechatronics Engineer", "IoT Specialist", "Drama Artist"];
let i = 0, j = 0, isDeleting = false;
const target = document.getElementById('typewriter');

function loopTyping() {
    if (!target) return;
    const current = phrases[i];
    
    if (isDeleting) {
        target.innerText = current.substring(0, j--);
    } else {
        target.innerText = current.substring(0, j++);
    }

    let speed = isDeleting ? 50 : 100;

    if (!isDeleting && j > current.length) {
        speed = 2000; 
        isDeleting = true;
        j = current.length; 
    } else if (isDeleting && j === 0) {
        isDeleting = false;
        i = (i + 1) % phrases.length;
        speed = 500; 
    }
    
    setTimeout(loopTyping, speed);
}
document.addEventListener('DOMContentLoaded', loopTyping);

// --- MOBILE MENU TOGGLE (ORIGINAL) ---
const toggleBtn = document.querySelector('.mobile-toggle');
const mobileMenu = document.querySelector('.mobile-menu');
const links = document.querySelectorAll('.mobile-link');

if (toggleBtn && mobileMenu) {
    toggleBtn.addEventListener('click', () => {
        mobileMenu.classList.toggle('active');
        toggleBtn.innerHTML = mobileMenu.classList.contains('active') ? '<i class="fas fa-times"></i>' : '<i class="fas fa-bars"></i>';
    });

    links.forEach(link => {
        link.addEventListener('click', () => {
            mobileMenu.classList.remove('active');
            toggleBtn.innerHTML = '<i class="fas fa-bars"></i>';
        });
    });
}

// --- ANIMATED BLINKING EYE FAVICON (ORIGINAL) ---
const faviconCanvas = document.createElement('canvas');
const faviconCtx = faviconCanvas.getContext('2d');
const faviconLink = document.getElementById('dynamic-favicon');

faviconCanvas.width = 32;
faviconCanvas.height = 32;

let eyeState = 'OPEN'; 
let eyeHeight = 14;    
let blinkSpeed = 2;    
let pupilOffset = 0;   
let pupilDirection = 0.5;

function drawFavicon() {
    faviconCtx.clearRect(0, 0, 32, 32);
    faviconCtx.fillStyle = '#0a0b10';
    faviconCtx.fillRect(0, 0, 32, 32);

    if (eyeState === 'CLOSING') {
        eyeHeight -= blinkSpeed;
        if (eyeHeight <= 2) {
            eyeHeight = 2;
            eyeState = 'OPENING';
        }
    } else if (eyeState === 'OPENING') {
        eyeHeight += blinkSpeed;
        if (eyeHeight >= 14) {
            eyeHeight = 14;
            eyeState = 'OPEN';
            setTimeout(() => { eyeState = 'CLOSING'; }, Math.random() * 4000 + 2000);
        }
    }

    if (eyeState === 'OPEN') {
        pupilOffset += pupilDirection;
        if (pupilOffset > 5 || pupilOffset < -5) pupilDirection *= -1;
    }

    const cx = 16;
    const cy = 16;

    faviconCtx.strokeStyle = '#00f3ff';
    faviconCtx.lineWidth = 2;
    faviconCtx.beginPath();
    faviconCtx.ellipse(cx, cy, 14, eyeHeight, 0, 0, Math.PI * 2);
    faviconCtx.stroke();

    if (eyeHeight > 4) {
        faviconCtx.fillStyle = '#00f3ff';
        faviconCtx.beginPath();
        faviconCtx.arc(cx + pupilOffset, cy, 3, 0, Math.PI * 2);
        faviconCtx.fill();
    }

    if (faviconLink) {
        faviconLink.href = faviconCanvas.toDataURL('image/png');
    }

    requestAnimationFrame(drawFavicon);
}

drawFavicon();
setTimeout(() => { eyeState = 'CLOSING'; }, 2000);