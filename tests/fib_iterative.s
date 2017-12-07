; if #0 = 0, just print 0
xor #0 $0 #1
jz #1 #15

sub #0 $1 #0

; f0 = 0, f1 = 1, i = 0
or $0 $0 #1
or $1 $0 #2
or $0 $0 #4

; iteration
add #1 #2 #3
or $0 #2 #1
or $0 #3 #2
add #4 $1 #4
xor #4 #0 #5
jz #5 #13
jgt #0 #6

; print result
print #3
halt

; print result if input = 0
print $0
halt
