class StressComments {

  -- linha de comentário com tokens falsos: class if then else fi <- <= =>

  (*
    comentário simples
  *)

  (*
    nível 1
    (*
      nível 2
      (*
        nível 3
        class Main inherits IO
        x <- 10;
        "string fake"
      *)
      fim nível 2
    *)
    fim nível 1
  *)

  x : Int <- 0;

  main() : Object {
    {
      x <- x + 1;  -- comentário no fim da linha
      x;
    }
  };
};