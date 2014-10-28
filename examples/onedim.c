#pragma scop
for (int i = 0; i < 42; i++)
  S(i);
for (int i = 0; i < 1; i++)
  for (int j = 0; j < 42; j++)
    S(i, j);
#pragma endscop
