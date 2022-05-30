module test1
(
    input logic a, b,
    output logic y, z
);

always_comb begin
    if (a < b) begin
        y = 0;
        z = 1;
    end
    else begin
        y = 1;
        z = 1;
    end
end

endmodule

module test
(
    input logic a, b,
    output logic y, z
);

always_comb begin
    if (a < b) begin
        y = 0;
        z = 1;
    end
    else begin
        y = 1;
        z = 1;
    end
end

endmodule
