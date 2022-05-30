module test
(
    input logic a, b,
    output logic y, z
);

always_comb begin
    if (a > b) begin
        y = $signed(a);
        z = 0;
    end
end

endmodule
