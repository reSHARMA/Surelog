module mydesign ( input  x, y, z,
                  output o);
endmodule

module tb_top;
    wire [1:0]  a;
    wire        b, c;
    mydesign d0  ( .x (a[0]),
                   .y (b),
                   .z (a[1]),
                   .o (c));
endmodule
