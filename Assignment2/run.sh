#!/bin/bash

OUTPUT_FILE="final_4.txt"
> "$OUTPUT_FILE"

EXP_NUM=1

for BETA in $(seq 1 10); do
    BETA_VAL=$((BETA * 100))  # Proper arithmetic evaluation
    echo "=== EXP $EXP_NUM | BETA = $BETA_VAL ===" >> "$OUTPUT_FILE"
    # Update the -DBETA macro in Makefile
    sed -i "s/-DBETA=[0-9]*/-DBETA=$BETA_VAL/" Makefile

    make clean > /dev/null
    make > /dev/null

    # Run xv6 and capture output of the 'priority' command
    expect <<EOF > temp_output.txt
log_user 0
spawn make qemu-nox
expect "xv6\$"
log_user 1
send "priority\r"
set timeout 60
expect "xv6\$"
send "\x01x"
expect eof
EOF

    # Extract lines between 'priority' and the next shell prompt
    awk '/priority/{flag=1; next} /xv6\$/{flag=0} flag' temp_output.txt >> "$OUTPUT_FILE"
    echo -e "\n" >> "$OUTPUT_FILE"

    ((EXP_NUM++))

    echo "------------------------------------" >> "$OUTPUT_FILE"
done

rm -f temp_output.txt
echo "All done. Output saved to $OUTPUT_FILE"
