#pragma once
inline unsigned host_disconnect_calls = 0, host_connect_calls = 0;
inline bool host_connect_supported = true;
inline bool tud_disconnect() { ++host_disconnect_calls; return host_connect_supported; }
inline bool tud_connect() { ++host_connect_calls; return host_connect_supported; }
