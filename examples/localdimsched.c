// IMPORTANT: The .scop file WAS MODIFIED from the one generated from this code
#pragma scop
for (int i = 0; i < N; i++)
  for (int j = 0; j < N; j++)
    S1(i, j);
#pragma endscop
