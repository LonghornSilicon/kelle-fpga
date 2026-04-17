# csim_only.tcl — run C simulation only (no synthesis).  Fast iteration.
# Usage: vitis_hls -f scripts/csim_only.tcl

set PART "xczu7ev-ffvc1156-2-e"
set TOP_DIR [file normalize [file dirname [info script]]/..]

proc csim_block {name top_func src_files tb_files} {
    global PART TOP_DIR
    puts "\n--- csim: $name ---"
    open_project -reset hls_${name}_csim
    set_top $top_func
    set_part $PART
    foreach f $src_files { add_files $f }
    foreach f $tb_files  { add_files -tb $f }
    open_solution "solution1"
    set_part $PART
    create_clock -period 3.33 -name default
    csim_design -clean
    close_project
}

csim_block "rsa" "rsa_matmul" \
    [list $TOP_DIR/hls/rsa/rsa.cpp] \
    [list $TOP_DIR/hls/rsa/rsa_tb.cpp]

csim_block "evictor" "evictor_find_min" \
    [list $TOP_DIR/hls/evictor/evictor.cpp] \
    [list $TOP_DIR/hls/evictor/evictor_tb.cpp]

csim_block "aerp" "aerp_insert" \
    [list $TOP_DIR/hls/aerp/aerp.cpp] \
    [list $TOP_DIR/hls/aerp/aerp_tb.cpp]

puts "\n===== C simulation complete ====="
