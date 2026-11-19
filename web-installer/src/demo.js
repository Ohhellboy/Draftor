import { startUI } from './ui.js';
import { SimulatedFlash, SIM_MAC } from './simulator.js';

const requested = new URLSearchParams(location.search).get('scenario') || 'app0';
const scenario = ['app0', 'app1', 'fresh', 'partition', 'pending', 'unknown', 'flash8', 'security', 'disconnect', 'verify'].includes(requested) ? requested : 'app0';
startUI({ simulation: true, scenario, expectedMac: SIM_MAC,
  requestDriver: async release => new SimulatedFlash(release, scenario),
});
