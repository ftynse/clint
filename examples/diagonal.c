#pragma scop
for (int i = 0; i < 2; i++) {
  for (int j = i; j < i+1; j++) {
    S(i, j);
  }
}

for (int i = 3; i < 42; i++) {
  for (int j = i; j < i+1; j++) {
    S(i, j);
  }
}
#pragma endscop
