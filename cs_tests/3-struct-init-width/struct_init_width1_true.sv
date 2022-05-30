logic [31:0] bar;
localparam pad_lp = 32;

typedef struct {
   logic [63:0] fract;
} foo;

module test;
wire [63:0] tmp = bar << pad_lp;
assign foo = '{fract: tmp };
endmodule
