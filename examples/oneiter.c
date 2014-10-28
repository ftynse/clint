#pragma scop
for (int i = 5; i < 6; i++)
  for (int j = 4; j < 5; j++)
    for (int k = 3; k < 4; k++)
      for (int l = 2; l < 3; l++)
        S(i, j);
#pragma endscop
