logic [31:0] bar;
localparam pad_lp = 32;

typedef struct {
   logic [63:0] fract;
} foo;

module test;
assign foo = '{fract: {bar, (pad_lp)'(0)}};
endmodule
