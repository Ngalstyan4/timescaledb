#!/bin/bash
bash setup.sh
MEM=${MEM:-200}
let "SHARED=$MEM/3"
let "CACHE=$MEM/2"
let "WORK=($MEM-$SHARED)/30"
let "MAINT=$MEM/10"
INSTALL_DIR=/Users/ngalstyan/Projects/timescaledev/postgres_installed
#  shared buffers = mem / 3 = 17G
#  effective_cache_size = mem / 2 = 25GB
#  work_mem =  (mem - shared_buffers) / (max_connections * 3)
#           =      33G / 30 = 1G
#  maintenance_work_mem = mem / 10 = 5GB
#

make install && (ulimit -v ${MEM}000; ulimit -a; PGDATA=${INSTALL_DIR}/data/ ${INSTALL_DIR}/bin/postgres   -D ${INSTALL_DIR}/data/ \
  -cshared_preload_libraries="timescaledb" -clog_min_duration_statement=-1 \
  -clog_line_prefix="%m [%p]: [%x] %u@%d" -clogging_collector=off -csynchronous_commit=off \
  -cmax_wal_size=10GB -clog_lock_waits=on \
  -cshared_buffers=${SHARED}MB -ceffective_cache_size=${CACHE}MB \
  -cwork_mem=${WORK}MB -cmaintenance_work_mem=${MAINT}MB \
  -cmax_files_per_process=100 -cmax_locks_per_transaction=256 \
  )
#  -csession_preload_libraries='auto_explain' \
#  -cauto_explain.log_min_duration='2s' \
#  -cauto_explain.log_nested_statements=1 \
#  -cauto_explain.log_analyze=1 \
