#include <esp_partition.h>
#include <cstring>

extern "C" esp_err_t __real_esp_partition_erase_range(const esp_partition_t*, size_t, size_t);

// Arduino's initArduino() normally formats the entire default NVS when it sees
// an unsupported NVS version or runs out of free pages. Preserve the existing
// identity, bond keys and settings so that a failed open remains recoverable.
// Routine sector erases performed by NVS garbage collection still go through.
extern "C" esp_err_t __wrap_esp_partition_erase_range(const esp_partition_t* partition,
                                                     size_t offset, size_t size) {
  if (partition && partition->type == ESP_PARTITION_TYPE_DATA &&
      partition->subtype == ESP_PARTITION_SUBTYPE_DATA_NVS &&
      std::strcmp(partition->label, "nvs") == 0 &&
      offset == 0 && size == partition->size) {
    return ESP_ERR_NOT_SUPPORTED;
  }
  return __real_esp_partition_erase_range(partition, offset, size);
}
