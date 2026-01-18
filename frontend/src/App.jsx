import React, { useState, useEffect, useRef } from 'react';
import axios from 'axios';
import { Shield, Zap, RefreshCw, AlertTriangle, Terminal, Circle } from 'lucide-react';
import { motion, AnimatePresence } from 'framer-motion';

const API_URL = import.meta.env.VITE_API_URL || 'http://localhost:8000/check';
const CAPACITY = 100;
const REFILL_RATE = 5; // tokens per second

function App() {
  const [userId, setUserId] = useState('user_123');
  const [tokens, setTokens] = useState(CAPACITY);
  const [logs, setLogs] = useState([]);
  const [stats, setStats] = useState({ allowed: 0, denied: 0 });
  const [isSimulating, setIsSimulating] = useState(false);
  const [animatingTokens, setAnimatingTokens] = useState([]);

  // Simulation of visual refill (frontend only for smooth UI)
  useEffect(() => {
    const interval = setInterval(() => {
      setTokens(prev => Math.min(CAPACITY, prev + (REFILL_RATE / 10)));
    }, 100);
    return () => clearInterval(interval);
  }, []);

  const handleRequest = async () => {
    try {
      const response = await axios.post(API_URL, { user_id: userId });
      const { allowed, remaining } = response.data;

      // Sync visual tokens with server response
      setTokens(remaining);

      if (allowed) {
        setStats(prev => ({ ...prev, allowed: prev.allowed + 1 }));
        addLog('ALLOWED', `Token used. Remaining: ${remaining}`);

        // Spawn a visual token that flies out
        const tokenId = Date.now() + Math.random();
        setAnimatingTokens(prev => [...prev, tokenId]);
        setTimeout(() => {
          setAnimatingTokens(prev => prev.filter(id => id !== tokenId));
        }, 800);
      } else {
        setStats(prev => ({ ...prev, denied: prev.denied + 1 }));
        addLog('DENIED', 'Rate limit exceeded!');
      }
    } catch (error) {
      addLog('ERROR', error.response?.data?.detail || 'Server Offline');
    }
  };

  const addLog = (type, message) => {
    const newLog = {
      id: Date.now(),
      type,
      message,
      time: new Date().toLocaleTimeString()
    };
    setLogs(prev => [newLog, ...prev].slice(0, 50));
  };

  const runBulkSimulation = async (count) => {
    setIsSimulating(true);
    for (let i = 0; i < count; i++) {
      await handleRequest();
      await new Promise(r => setTimeout(r, 80)); // small delay to see it happen
    }
    setIsSimulating(false);
  };

  return (
    <div className="App">
      <header style={{ marginBottom: '3rem' }}>
        <motion.div
          initial={{ y: -20, opacity: 0 }}
          animate={{ y: 0, opacity: 1 }}
          style={{ display: 'flex', alignItems: 'center', justifyContent: 'center', gap: '1rem' }}
        >
          <Shield size={40} color="#38bdf8" />
          <h1 style={{ fontSize: '2.5rem', margin: 0 }}>Distributed Rate Limiter</h1>
        </motion.div>
        <p style={{ color: 'rgba(255,255,255,0.6)', marginTop: '0.5rem' }}>
          Real-time Visualization of the Token Bucket Algorithm (C++ & gRPC)
        </p>
      </header>

      <div style={{ display: 'grid', gridTemplateColumns: 'minmax(400px, 1fr) 1fr', gap: '2rem', textAlign: 'left' }}>
        {/* Left Side: Visualization */}
        <div className="glass-card">
          <h2 style={{ display: 'flex', alignItems: 'center', gap: '0.5rem' }}>
            <Zap size={20} color="#38bdf8" /> The Token Bucket
          </h2>

          <div style={{ position: 'relative', width: 'fit-content', margin: '0 auto' }}>
            {/* Tapered Bucket */}
            <div className={`token-bucket-container ${tokens < 1 ? 'empty' : ''}`}>
              <div
                className="liquid"
                style={{ height: `${(tokens / CAPACITY) * 100}%` }}
              />
            </div>

            {/* Token Ejection Animation */}
            <AnimatePresence>
              {animatingTokens.map(id => (
                <motion.div
                  key={id}
                  initial={{ bottom: '10%', opacity: 1, x: 0, scale: 1 }}
                  animate={{ bottom: '100%', opacity: 0, x: 50, scale: 0.5, rotate: 45 }}
                  exit={{ opacity: 0 }}
                  transition={{ duration: 0.8, ease: "easeOut" }}
                  style={{
                    position: 'absolute',
                    left: '45%',
                    zIndex: 10,
                    color: '#38bdf8'
                  }}
                >
                  <Circle size={30} fill="#38bdf8" />
                </motion.div>
              ))}
            </AnimatePresence>
          </div>

          <div className="stats-grid">
            <div className="stat-item">
              <span className={`stat-value ${tokens < 1 ? 'danger' : ''}`}>
                {Math.floor(tokens)}
              </span>
              <span className="stat-label">Available Tokens</span>
            </div>
            <div className="stat-item">
              <span className="stat-value">{CAPACITY}</span>
              <span className="stat-label">Max Capacity</span>
            </div>
            <div className="stat-item">
              <span className="stat-value">{REFILL_RATE}/s</span>
              <span className="stat-label">Refill Rate</span>
            </div>
          </div>
        </div>

        {/* Right Side: Controls & Logs */}
        <div className="glass-card">
          <h2 style={{ display: 'flex', alignItems: 'center', gap: '0.5rem' }}>
            <Terminal size={20} color="#38bdf8" /> Simulation Controls
          </h2>

          <div style={{ marginBottom: '2rem' }}>
            <label style={{ display: 'block', marginBottom: '0.5rem', color: 'rgba(255,255,255,0.6)' }}>User ID</label>
            <input
              type="text"
              value={userId}
              onChange={(e) => setUserId(e.target.value)}
              style={{
                width: '100%',
                padding: '0.8rem',
                borderRadius: '8px',
                background: 'rgba(0,0,0,0.3)',
                border: '1px solid rgba(255,255,255,0.1)',
                color: 'white',
                fontSize: '1rem'
              }}
            />
          </div>

          <div style={{ display: 'flex', flexWrap: 'wrap', gap: '0.5rem' }}>
            <button onClick={() => handleRequest()} disabled={isSimulating}>
              Hit API (1)
            </button>
            <button onClick={() => runBulkSimulation(50)} disabled={isSimulating}>
              Burst Test (50)
            </button>
            <button onClick={() => runBulkSimulation(500)} className="denied" disabled={isSimulating}>
              Flood Attack (500)
            </button>
          </div>

          <div className="stats-grid" style={{ marginTop: '1rem', borderTop: '1px solid rgba(255,255,255,0.1)', paddingTop: '1rem' }}>
            <div className="stat-item">
              <span className="stat-value" style={{ color: '#4ade80' }}>{stats.allowed}</span>
              <span className="stat-label">Successful</span>
            </div>
            <div className="stat-item">
              <span className="stat-value" style={{ color: '#f87171' }}>{stats.denied}</span>
              <span className="stat-label">Rate Limited</span>
            </div>
          </div>

          <div className="log-container">
            <AnimatePresence>
              {logs.map(log => (
                <motion.div
                  key={log.id}
                  initial={{ x: -10, opacity: 0 }}
                  animate={{ x: 0, opacity: 1 }}
                  className={`log-entry ${log.type.toLowerCase()}`}
                >
                  [{log.time}] {log.type}: {log.message}
                </motion.div>
              ))}
            </AnimatePresence>
          </div>
        </div>
      </div>

      <footer style={{ marginTop: '3rem', color: 'rgba(255,255,255,0.3)', fontSize: '0.875rem' }}>
        Built with C++17, gRPC, Redis, and React
      </footer>
    </div>
  );
}

export default App;
