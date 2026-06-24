// =============================================================================
// TopWrapper.v - Nexys Video FPGA Top Level Wrapper
// Connects Chisel SoCTop with Xilinx MMCM Clocking and HDMI TMDS Transmitters
// =============================================================================

module TopWrapper (
    input  wire       sys_clk,        // 100 MHz board clock

    // UART Connection
    output wire       uart_tx,
    input  wire       uart_rx,

    // Onboard I/O
    output wire [7:0] led,
    input  wire [4:0] btn,            // Buttons: Center (btnC), Left (btnL), Right (btnR), Down (btnD), Up (btnU)

    // HDMI TX output
    output wire       hdmi_tx_clk_p,
    output wire       hdmi_tx_clk_n,
    output wire [2:0] hdmi_tx_p,
    output wire [2:0] hdmi_tx_n
);

    // =========================================================================
    // 1. Clock Generation using Xilinx MMCM
    // Generates 25 MHz (Pixel Clock) and 125 MHz (5x Clock for Serializer)
    // =========================================================================
    wire clk_25m;
    wire clk_125m;
    wire mmcm_fb, mmcm_fb_buf;
    wire clk_25m_unbuf, clk_125m_unbuf;
    wire locked;

    MMCME2_BASE #(
        .CLKFBOUT_MULT_F  (10.0),       // VCO = 100 MHz * 10 = 1000 MHz
        .CLKIN1_PERIOD     (10.0),      // 100 MHz input period
        .CLKOUT0_DIVIDE_F  (40.0),      // CLKOUT0 = 1000 / 40 = 25 MHz (Pixel Clock)
        .CLKOUT1_DIVIDE    (8),         // CLKOUT1 = 1000 / 8 = 125 MHz (5x Clock)
        .DIVCLK_DIVIDE     (1)
    ) mmcm_inst (
        .CLKFBOUT  (mmcm_fb),
        .CLKFBIN   (mmcm_fb_buf),
        .CLKIN1    (sys_clk),
        .CLKOUT0   (clk_25m_unbuf),
        .CLKOUT1   (clk_125m_unbuf),
        .CLKOUT2   (), .CLKOUT3 (), .CLKOUT4 (),
        .CLKOUT5   (), .CLKOUT6 (),
        .CLKFBOUTB (), .CLKOUT0B (),
        .LOCKED    (locked),
        .PWRDWN    (1'b0),
        .RST       (1'b0)
    );

    // Global Clock Buffers
    BUFG bufg_fb   (.I(mmcm_fb), .O(mmcm_fb_buf));
    BUFG bufg_25m  (.I(clk_25m_unbuf),  .O(clk_25m));
    BUFG bufg_125m (.I(clk_125m_unbuf), .O(clk_125m));

    // Reset Generator (Release reset after MMCM locks)
    reg [7:0] rst_cnt = 8'd0;
    wire rst = !rst_cnt[7];
    always @(posedge clk_25m) begin
        if (!locked)
            rst_cnt <= 8'd0;
        else if (!rst_cnt[7])
            rst_cnt <= rst_cnt + 8'd1;
    end

    // =========================================================================
    // 2. Chisel SoC Top Module Instantiation
    // =========================================================================
    wire        soc_hsync;
    wire        soc_vsync;
    wire        soc_vde;
    wire [23:0] soc_rgb;

    SoCTop soc_inst (
        .clock            (clk_25m),          // CPU clocks at 25 MHz
        .reset            (rst),
        
        .io_uartTx        (uart_tx),
        .io_uartRx        (uart_rx),
        
        .io_leds          (led),
        .io_buttons       (btn),
        
        .io_videoPixelClk (clk_25m),
        .io_videoHsync    (soc_hsync),
        .io_videoVsync    (soc_vsync),
        .io_videoVde      (soc_vde),
        .io_videoRgb      (soc_rgb)
    );

    // =========================================================================
    // 3. HDMI TMDS Channel Encoders & Serializers
    // Channel 0 -> Blue, Channel 1 -> Green, Channel 2 -> Red
    // =========================================================================
    
    // Channel 0 (Blue): Encodes blue byte and carries hsync/vsync control bits
    tmds_channel_encoder ch0_blue (
        .clk_pixel    (clk_25m),
        .clk_pixel_5x (clk_125m),
        .reset        (rst),
        .pixel_data   (soc_rgb[7:0]),
        .de           (soc_vde),
        .ctrl0        (soc_hsync),
        .ctrl1        (soc_vsync),
        .tmds_p       (hdmi_tx_p[0]),
        .tmds_n       (hdmi_tx_n[0])
    );

    // Channel 1 (Green): Encodes green byte, control bits are 0
    tmds_channel_encoder ch1_green (
        .clk_pixel    (clk_25m),
        .clk_pixel_5x (clk_125m),
        .reset        (rst),
        .pixel_data   (soc_rgb[15:8]),
        .de           (soc_vde),
        .ctrl0        (1'b0),
        .ctrl1        (1'b0),
        .tmds_p       (hdmi_tx_p[1]),
        .tmds_n       (hdmi_tx_n[1])
    );

    // Channel 2 (Red): Encodes red byte, control bits are 0
    tmds_channel_encoder ch2_red (
        .clk_pixel    (clk_25m),
        .clk_pixel_5x (clk_125m),
        .reset        (rst),
        .pixel_data   (soc_rgb[23:16]),
        .de           (soc_vde),
        .ctrl0        (1'b0),
        .ctrl1        (1'b0),
        .tmds_p       (hdmi_tx_p[2]),
        .tmds_n       (hdmi_tx_n[2])
    );

    // =========================================================================
    // 4. HDMI Clock Output via ODDR (Phase-Aligned to Data)
    // =========================================================================
    wire hdmi_clk_out;
    
    ODDR #(
        .DDR_CLK_EDGE("SAME_EDGE"), 
        .INIT(1'b0),    
        .SRTYPE("SYNC") 
    ) ODDR_hdmi_clk (
        .Q(hdmi_clk_out),   
        .C(clk_25m),   
        .CE(1'b1), 
        .D1(1'b1), 
        .D2(1'b0), 
        .R(1'b0),   
        .S(1'b0)    
    );

    OBUFDS hdmi_clk_buf (
        .I  (hdmi_clk_out),
        .O  (hdmi_tx_clk_p),
        .OB (hdmi_tx_clk_n)
    );

endmodule
