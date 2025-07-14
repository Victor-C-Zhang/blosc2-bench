#include <assert.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>

#include "blosc2.h"
#include <blosc2/tuners-registry.h>
#include <btune.h>

static constexpr double MiB = 1024. * 1024.;
static constexpr int NUM_CHUNKS = 50; // give BTune a reasonable number of iterations to come online

long get_file_size(FILE *file) {
  fseek(file, 0, SEEK_END);
  long length = ftell(file);
  rewind(file); // Reset the file pointer to the beginning
  return length;
}
// Function to copy a file to a buffer
void copy_file_to_buffer(FILE *file, char *buffer, long length) {
  size_t bytes_read = fread(buffer, 1, length, file);
  if (bytes_read != length) {
    fprintf(stderr, "Error reading file: expected %ld bytes, read %zu bytes\n",
            length, bytes_read);
    exit(EXIT_FAILURE);
  }
}

static int round_trip(const char *in_fname, std::ofstream &statsFile) {
  // Open input file
  FILE *in_file = fopen(in_fname, "rb");
  if (in_file == NULL) {
    fprintf(stderr, "Input file cannot be opened.\n");
    return EXIT_FAILURE;
  }
  long file_size = get_file_size(in_file);
  char *buffer = (char *)malloc(
      file_size +
      1); // Allocate space for the file contents and a null terminator
  if (!buffer) {
    fprintf(stderr, "Error allocating memory for file buffer\n");
    fclose(in_file);
    return EXIT_FAILURE;
  }
  copy_file_to_buffer(in_file, buffer, file_size);
  buffer[file_size] = '\0'; // Add a null terminator to the end of the buffer
  fclose(in_file);

  // compression params
  blosc2_cparams cparams = BLOSC2_CPARAMS_DEFAULTS;
  cparams.nthreads = 1; // Btune may lower this
  cparams.typesize = 8;

  // btune
  btune_config btune_config = BTUNE_CONFIG_DEFAULTS;
  // btune_config.perf_mode = BTUNE_PERF_DECOMP;
  btune_config.tradeoff[0] = .9;
  btune_config.tradeoff_nelems = 1;
  /* For lossy mode it would be
  btune_config.tradeoff[0] = .5;
  btune_config.tradeoff[1] = .2;
  btune_config.tradeoff[2] = .3;
  btune_config.tradeoff_nelems = 3;
  */
  btune_config.behaviour.nhards_before_stop = 10;
  btune_config.behaviour.repeat_mode = BTUNE_REPEAT_ALL;
  btune_config.use_inference = 2;
  // char *models_dir = "./models/";
  // strcpy(btune_config.models_dir, models_dir);
  cparams.tuner_id = BLOSC_BTUNE;
  cparams.tuner_params = &btune_config;

  // Create super chunk
  blosc2_dparams dparams = BLOSC2_DPARAMS_DEFAULTS;
  dparams.nthreads = 1;
  blosc2_storage storage = {
      .contiguous = true,
      .urlpath = NULL,
      .cparams = &cparams,
      .dparams = &dparams,
  };
  blosc2_schunk *schunk_out = blosc2_schunk_new(&storage);
  if (schunk_out == NULL) {
    fprintf(stderr, "Output file cannot be created.\n");
    return 1;
  }

  // Statistics
  std::chrono::duration<double, std::milli> ctimeMs{};
  std::chrono::duration<double, std::milli> dtimeMs{};

  // Compress
  int chunkSize = file_size / NUM_CHUNKS;
  chunkSize = chunkSize / 8 * 8; // round down to multiple of 8
  int leftover = file_size - chunkSize * NUM_CHUNKS;
  for (int i = 0; i < NUM_CHUNKS; i++) {
    auto start = std::chrono::high_resolution_clock::now();
    const int res = blosc2_schunk_append_buffer(
        schunk_out, buffer + chunkSize * i, chunkSize);
    auto end = std::chrono::high_resolution_clock::now();
    if (res != i + 1) {
      fprintf(stderr, "Error in appending data to destination file");
      return 1;
    }
    ctimeMs += end - start;
  }
  if (leftover > 0) {
    auto start = std::chrono::high_resolution_clock::now();
    const int res = blosc2_schunk_append_buffer(
        schunk_out, buffer + chunkSize * NUM_CHUNKS, leftover);
    auto end = std::chrono::high_resolution_clock::now();
    if (res != NUM_CHUNKS + 1) {
      fprintf(stderr, "Error in appending (leftover) data to destination file");
      return 1;
    }
    ctimeMs += end - start;
  }

  // Decompress
  char *regen = (char *)malloc(file_size + 1);
  regen[file_size] = '\0';
  for (int i = 0; i < NUM_CHUNKS; i++) {
    auto start = std::chrono::high_resolution_clock::now();
    int size = blosc2_schunk_decompress_chunk(schunk_out, i,
                                              regen + chunkSize * i, chunkSize);
    auto end = std::chrono::high_resolution_clock::now();
    if (size != chunkSize) {
      std::cerr << "Error in decompressing data: [" << i << "] " << size << " != " << chunkSize << std::endl;
      return 1;
    }
    dtimeMs += end - start;
  }
  if (leftover > 0) {
    auto start = std::chrono::high_resolution_clock::now();
    int size = blosc2_schunk_decompress_chunk(
        schunk_out, NUM_CHUNKS, regen + chunkSize * NUM_CHUNKS, leftover);
    auto end = std::chrono::high_resolution_clock::now();
    if (size != leftover) {
      fprintf(stderr, "Error in decompressing (leftover) data");
      return 1;
    }
    dtimeMs += end - start;
  }

  if (memcmp(buffer, regen, file_size) != 0) {
    fprintf(stderr, "Roundtrip failed!");
    return 1;
  }

  // Statistics
  int64_t nbytes = schunk_out->nbytes;
  int64_t cbytes = schunk_out->cbytes;
  statsFile << nbytes << "," << cbytes << "," << (double)nbytes / (double)cbytes
            << "," << ctimeMs.count() << "," << dtimeMs.count() << ","
            << in_fname << std::endl;
  // std::cout << "Compression ratio: " << nbytes << " -> " << cbytes << " (" <<
  // (1. * (double)nbytes) / (double)cbytes << "x)" << std::endl; std::cout <<
  // "Compression time: " << ctimeMs.count() << " ms, " << (double)nbytes * 1000
  // / (ctimeMs.count() * MiB) << " MiB/s" << std::endl; std::cout <<
  // "Decompression time: " << dtimeMs.count() << " ms, " << (double)nbytes *
  // 1000 / (dtimeMs.count() * MiB) << " MiB/s" << std::endl;

  // Free resources
  blosc2_schunk_free(schunk_out);
  free(buffer);
  free(regen);
  return 0;
}

static int round_trip_dir(const char *in_dir) {
  std::filesystem::path dirPath(in_dir);
  if (!std::filesystem::exists(dirPath)) {
    fprintf(stderr, "Input directory does not exist.\n");
    return 1;
  }
  std::ofstream statsFile{dirPath / "stats.txt"};
  statsFile << "srcSize,compressedSize,compressionRatio,ctimeMs,dtimeMs,srcFile"
            << std::endl;

  for (const auto &entry : std::filesystem::directory_iterator(dirPath)) {
    if (entry.is_regular_file() && entry.path().filename() != "stats.txt") {
      const char *fname = entry.path().c_str();
      // printf("Processing file: %s\n", fname);
      int res = round_trip(fname, statsFile);
      if (res != 0) {
        fprintf(stderr, "Error file: %s\n", fname);
      }
    }
  }
  statsFile.close();

  return 0;
}

int main(int argc, char *argv[]) {
  blosc2_init();

  // Input parameters
  if (argc != 2) {
    fprintf(stderr, "./roundtrip <input dir>\n");
    return 1;
  }

  const char *in_fname = argv[1];
  const int ret = round_trip_dir(in_fname);
  // auto devnull = std::ofstream("/dev/null");
  // const int ret = round_trip(in_fname, devnull);

  blosc2_destroy();

  if (ret != 0) {
    fprintf(stderr, "Error in round trip\n");
  }
  return ret;
}
