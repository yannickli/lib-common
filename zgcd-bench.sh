#!/bin/sh

perf record ./zgcd-bench $@ && perf report
