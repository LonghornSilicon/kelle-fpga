# ZCU104 constraints for Kelle accelerator IP blocks
# Target: xczu7ev-ffvc1156-2-e (ZCU104)
# PL clock: 300 MHz from PS (or PL PLL)
#
# NOTE: Pin assignments are for standalone PL use.
# When integrated with PS via AXI, use the Vivado block design
# and let the tool assign HP/HPC port connections automatically.

# --- Primary PL clock (300 MHz) ---
# Driven from PS FCLK_CLK0 in block design; define timing constraint here
# for standalone HLS IP timing analysis.
create_clock -period 3.333 -name pl_clk0 [get_ports pl_clk0]

# --- Timing exceptions for AXI interfaces ---
# Allow up to 2 cycles for AXI handshaking across clock-domain boundaries
set_max_delay -datapath_only 6.667 \
    -from [get_cells -hierarchical -filter {NAME =~ *s_axilite*}] \
    -to   [get_cells -hierarchical -filter {NAME =~ *m_axi*}]

# --- BRAM pipelining (improves timing on RAMB36E2) ---
set_property RAM_STYLE BLOCK [get_cells -hierarchical -filter {RAM_STYLE == auto}]

# --- I/O standards (for PMOD debug pins, optional) ---
# set_property PACKAGE_PIN H12 [get_ports {debug[0]}]
# set_property IOSTANDARD LVCMOS33 [get_ports {debug[0]}]
