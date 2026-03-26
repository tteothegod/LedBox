import React from 'react';
import './App.css';
import StatusPanel from './components/StatusPanel';
import ConfigForm from './components/ConfigForm';
import StreamView from './components/StreamView';

function App() {
  return (
    <div className="app-root">
      <aside className="sidebar">
        <div className="brand">LED Sync Box</div>
        <nav>
          <div className="nav-item">Dashboard</div>
          <div className="nav-item">Configuration</div>
          <div className="nav-item">Live View</div>
        </nav>
        <div className="sidebar-foot">Pi Zero 2W · Local UI</div>
      </aside>

      <main className="main">
        <header className="main-header">
          <h1>LED Sync Box</h1>
          <p className="sub">Status, configuration and live preview</p>
        </header>

        <section className="grid">
          <div className="card">
            <h2>System Status</h2>
            <StatusPanel />
          </div>

          <div className="card">
            <h2>Live Camera</h2>
            <StreamView />
          </div>

          <div className="card wide">
            <h2>Configuration</h2>
            <ConfigForm />
          </div>
        </section>

        <footer className="footer">Built for Raspberry Pi · Control via REST endpoints</footer>
      </main>
    </div>
  );
}

export default App;
