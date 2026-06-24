// =============================================================================
// TMDS Channel Encoder - Single channel HDMI TX
// 8b-to-10b TMDS encoding + OSERDESE2 serialization
// Nexys Video (Artix-7 XC7A200T)
// =============================================================================

module tmds_channel_encoder (
    input  wire       clk_pixel,    // Pixel clock (1x)
    input  wire       clk_pixel_5x, // 5x pixel clock for OSERDES
    input  wire       reset,
    input  wire [7:0] pixel_data,   // 8-bit pixel data to encode
    input  wire       de,           // Data Enable
    input  wire       ctrl0,        // Control bit 0 (hsync for ch0)
    input  wire       ctrl1,        // Control bit 1 (vsync for ch0)
    output wire       tmds_p,       // TMDS differential positive output
    output wire       tmds_n        // TMDS differential negative output
);

    // =========================================================================
    // TMDS 8b-to-10b Encoder (DVI 1.0 spec)
    // =========================================================================
    reg [9:0] tmds_word;
    reg signed [4:0] dc_bias; // DC balance counter

    // Count number of 1s in input
    wire [3:0] n_ones = pixel_data[0] + pixel_data[1] + pixel_data[2] +
                        pixel_data[3] + pixel_data[4] + pixel_data[5] +
                        pixel_data[6] + pixel_data[7];

    // Stage 1: XOR or XNOR based on number of 1s
    wire use_xnor = (n_ones > 4) || (n_ones == 4 && pixel_data[0] == 0);

    wire [8:0] q_m;
    assign q_m[0] = pixel_data[0];
    assign q_m[1] = use_xnor ? ~(pixel_data[1] ^ q_m[0]) : (pixel_data[1] ^ q_m[0]);
    assign q_m[2] = use_xnor ? ~(pixel_data[2] ^ q_m[1]) : (pixel_data[2] ^ q_m[1]);
    assign q_m[3] = use_xnor ? ~(pixel_data[3] ^ q_m[2]) : (pixel_data[3] ^ q_m[2]);
    assign q_m[4] = use_xnor ? ~(pixel_data[4] ^ q_m[3]) : (pixel_data[4] ^ q_m[3]);
    assign q_m[5] = use_xnor ? ~(pixel_data[5] ^ q_m[4]) : (pixel_data[5] ^ q_m[4]);
    assign q_m[6] = use_xnor ? ~(pixel_data[6] ^ q_m[5]) : (pixel_data[6] ^ q_m[5]);
    assign q_m[7] = use_xnor ? ~(pixel_data[7] ^ q_m[6]) : (pixel_data[7] ^ q_m[6]);
    assign q_m[8] = use_xnor ? 1'b0 : 1'b1;

    // Count 1s and 0s in q_m[7:0]
    wire [3:0] n1_qm = q_m[0] + q_m[1] + q_m[2] + q_m[3] +
                        q_m[4] + q_m[5] + q_m[6] + q_m[7];
    wire [3:0] n0_qm = 4'd8 - n1_qm;

    // Stage 2: DC balance
    always @(posedge clk_pixel) begin
        if (reset) begin
            tmds_word <= 10'd0;
            dc_bias   <= 5'sd0;
        end else if (!de) begin
            // Control period - send control tokens
            dc_bias <= 5'sd0;
            case ({ctrl1, ctrl0})
                2'b00: tmds_word <= 10'b1101010100;
                2'b01: tmds_word <= 10'b0010101011;
                2'b10: tmds_word <= 10'b0101010100;
                2'b11: tmds_word <= 10'b1010101011;
            endcase
        end else begin
            if (dc_bias == 5'sd0 || n1_qm == n0_qm) begin
                // No bias or equal 1s/0s
                tmds_word[9]   <= ~q_m[8];
                tmds_word[8]   <= q_m[8];
                tmds_word[7:0] <= q_m[8] ? q_m[7:0] : ~q_m[7:0];
                if (q_m[8] == 1'b0)
                    dc_bias <= dc_bias + ($signed({1'b0, n0_qm}) - $signed({1'b0, n1_qm}));
                else
                    dc_bias <= dc_bias + ($signed({1'b0, n1_qm}) - $signed({1'b0, n0_qm}));
            end else begin
                if ((dc_bias > 5'sd0 && n1_qm > n0_qm) ||
                    (dc_bias < 5'sd0 && n0_qm > n1_qm)) begin
                    // Invert to balance
                    tmds_word[9]   <= 1'b1;
                    tmds_word[8]   <= q_m[8];
                    tmds_word[7:0] <= ~q_m[7:0];
                    dc_bias <= dc_bias + {q_m[8], 1'b0} +
                               ($signed({1'b0, n0_qm}) - $signed({1'b0, n1_qm}));
                end else begin
                    // Don't invert
                    tmds_word[9]   <= 1'b0;
                    tmds_word[8]   <= q_m[8];
                    tmds_word[7:0] <= q_m[7:0];
                    dc_bias <= dc_bias - {~q_m[8], 1'b0} +
                               ($signed({1'b0, n1_qm}) - $signed({1'b0, n0_qm}));
                end
            end
        end
    end

    // =========================================================================
    // OSERDESE2 - 10:1 serialization (two cascaded 5:1 in DDR mode)
    // =========================================================================
    wire tmds_serial;
    wire shift_out1, shift_out2;

    // Master OSERDES
    OSERDESE2 #(
        .DATA_RATE_OQ ("DDR"),
        .DATA_RATE_TQ ("SDR"),
        .DATA_WIDTH   (10),
        .SERDES_MODE  ("MASTER"),
        .TRISTATE_WIDTH (1)
    ) oserdes_master (
        .OQ       (tmds_serial),
        .OFB      (),
        .TQ       (),
        .TFB      (),
        .SHIFTOUT1(),
        .SHIFTOUT2(),
        .CLK      (clk_pixel_5x),
        .CLKDIV   (clk_pixel),
        .D1       (tmds_word[0]),
        .D2       (tmds_word[1]),
        .D3       (tmds_word[2]),
        .D4       (tmds_word[3]),
        .D5       (tmds_word[4]),
        .D6       (tmds_word[5]),
        .D7       (tmds_word[6]),
        .D8       (tmds_word[7]),
        .TCE      (1'b0),
        .OCE      (1'b1),
        .TBYTEIN  (1'b0),
        .TBYTEOUT (),
        .RST      (reset),
        .SHIFTIN1 (shift_out1),
        .SHIFTIN2 (shift_out2),
        .T1       (1'b0),
        .T2       (1'b0),
        .T3       (1'b0),
        .T4       (1'b0)
    );

    // Slave OSERDES
    OSERDESE2 #(
        .DATA_RATE_OQ ("DDR"),
        .DATA_RATE_TQ ("SDR"),
        .DATA_WIDTH   (10),
        .SERDES_MODE  ("SLAVE"),
        .TRISTATE_WIDTH (1)
    ) oserdes_slave (
        .OQ       (),
        .OFB      (),
        .TQ       (),
        .TFB      (),
        .SHIFTOUT1(shift_out1),
        .SHIFTOUT2(shift_out2),
        .CLK      (clk_pixel_5x),
        .CLKDIV   (clk_pixel),
        .D1       (1'b0),
        .D2       (1'b0),
        .D3       (tmds_word[8]),
        .D4       (tmds_word[9]),
        .D5       (1'b0),
        .D6       (1'b0),
        .D7       (1'b0),
        .D8       (1'b0),
        .TCE      (1'b0),
        .OCE      (1'b1),
        .TBYTEIN  (1'b0),
        .TBYTEOUT (),
        .RST      (reset),
        .SHIFTIN1 (1'b0),
        .SHIFTIN2 (1'b0),
        .T1       (1'b0),
        .T2       (1'b0),
        .T3       (1'b0),
        .T4       (1'b0)
    );

    // =========================================================================
    // Differential output buffer
    // =========================================================================
    OBUFDS #(
        .IOSTANDARD("TMDS_33")
    ) obufds_inst (
        .I  (tmds_serial),
        .O  (tmds_p),
        .OB (tmds_n)
    );

endmodule
