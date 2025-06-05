# Jemalloc Heap Profiling in Fluent Bit

Fluent Bit supports heap profiling through jemalloc when compiled with the appropriate flags. This feature allows you to analyze memory allocation patterns and identify potential memory leaks or optimization opportunities.

## Building with Heap Profiling Support

To enable heap profiling, you need to build Fluent Bit with both jemalloc and jemalloc profiling support:

```bash
cmake -DFLB_JEMALLOC=ON -DFLB_JEMALLOC_PROFILING=ON -DFLB_HTTP_SERVER=ON ..
make
```

## Runtime Configuration

### Enable Profiling

To enable heap profiling at runtime, set the `MALLOC_CONF` environment variable:

```bash
MALLOC_CONF="prof:true" fluent-bit -c your-config.conf
```

To grab a profile:

```bash
curl http://localhost:2020/api/v1/heap_profile > heap.prof
```

To view it as a flamegraph:
```bash
jeprof --show_bytes `which fluent-bit` heap.prof  --collapsed | flamegraph.pl >prof.svg
```
