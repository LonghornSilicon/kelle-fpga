# run_hls.tcl — synthesize and csim all Kelle HLS blocks for ZCU104
# Usage: vitis_hls -f scripts/run_hls.tcl
# Or from Vitis HLS GUI: Tools > Run Tcl Script

set PART "xczu7ev-ffvc1156-2-e"
set CLK_PERIOD "3.33"  ;# 300 MHz target (conservative for ZCU104 PL)
set TOP_DIR [file normalize [file dirname [info script]]/..]

proc run_block {name top_func src_files tb_files} {
    global PART CLK_PERIOD TOP_DIR

    puts "\n========================================="
    puts " Building: $name"
    puts "========================================="

    open_project -reset hls_${name}
    set_top $top_func
    set_part $PART

    foreach f $src_files { add_files $f }
    foreach f $tb_files  { add_files -tb $f }

    open_solution -reset "solution1"
    set_part $PART
    create_clock -period $CLK_PERIOD -name default

    # Directives matching the #pragma HLS in source
    config_interface -m_axi_addr64
    config_compile -pipeline_loops 0

    puts "\n--- C Simulation ---"
    csim_design -clean

    puts "\n--- C Synthesis ---"
    csynth_design

    puts "\n--- Co-simulation (RTL) ---"
    cosim_design -rtl verilog

    puts "\n--- Export IP ---"
    export_design -format ip_catalog -description "Kelle $name block"

    close_project
}

# RSA — systolic array
run_block "rsa" "rsa_matmul" \
    [list $TOP_DIR/hls/rsa/rsa.cpp] \
    [list $TOP_DIR/hls/rsa/rsa_tb.cpp]

# Systolic Evictor
run_block "evictor" "evictor_find_min" \
    [list $TOP_DIR/hls/evictor/evictor.cpp] \
    [list $TOP_DIR/hls/evictor/evictor_tb.cpp]

# AERP
run_block "aerp" "aerp_insert" \
    [list $TOP_DIR/hls/aerp/aerp.cpp] \
    [list $TOP_DIR/hls/aerp/aerp_tb.cpp]

puts "\n===== All blocks complete ====="
