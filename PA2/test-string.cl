class StressStrings {
  a : String <- "simple";
  b : String <- "with spaces and numbers 1234567890";
  c : String <- "line\nbreak";
  d : String <- "tab\tseparated";
  e : String <- "backspace\btest";
  f : String <- "formfeed\ftest";
  g : String <- "quote \" inside";
  h : String <- "backslash \\ inside";
  i : String <- "symbols !@#$%^&*()_+-={}[]:;,.?/<>|~";
  j : String <- "mixed \n\t\b\f chars";
  k : String <- "this string \
continues on next line";
  l : String <- "01234567890123456789012345678901234567890123456789";
  m : String <- "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

  main() : Object {
    {
      a; b; c; d; e; f; g; h; i; j; k; l; m;
    }
  };
};