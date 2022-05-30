module foo #(localparam happy_lp=1)
  (output o);
  assign o = $signed(happy_lp);
endmodule
