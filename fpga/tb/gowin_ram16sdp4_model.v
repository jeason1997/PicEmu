/* 仅供 Icarus 使用的 Gowin RAM16SDP4 行为模型；综合使用器件原语。 */
module \$__GOWIN_LUTRAM_ (PORT_W_CLK, PORT_W_ADDR, PORT_W_WR_EN,
                          PORT_W_WR_DATA, PORT_R_ADDR, PORT_R_RD_DATA);
    parameter [63:0] INIT=64'b0;
    parameter [3:0] BITS_USED=4'b1111;
    output [3:0] PORT_R_RD_DATA;
    input [3:0] PORT_W_WR_DATA, PORT_W_ADDR, PORT_R_ADDR;
    input PORT_W_WR_EN, PORT_W_CLK;
    reg [3:0] memory [0:15];
    integer i;
    initial begin
        for(i=0;i<16;i=i+1)
            memory[i]=INIT[i*4 +: 4];
    end
    assign PORT_R_RD_DATA=memory[PORT_R_ADDR];
    always @(posedge PORT_W_CLK)
        if(PORT_W_WR_EN) memory[PORT_W_ADDR]<=PORT_W_WR_DATA;
endmodule
