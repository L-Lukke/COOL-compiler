class A inherits IO {
  x : Int <- 0;
  y : Bool <- true;
  z : Bool <- false;
  s : String <- "abc";

  f(a : Int, b : Int) : Int {
    {
      a + b;
      a - b;
      a * b;
      a / b;
      a < b;
      a <= b;
      a = b;
      ~a;
    }
  };

  g() : Object {
    if isvoid s then
      new Object
    else
      let n : Int <- 5, m : Int <- 10 in
        while n < m loop
          n <- n + 1
        pool
    fi
  };

  h() : Object {
    case x of
      i : Int => i + 1;
      o : Object => 0;
    esac
  };

  main() : SELF_TYPE {
    {
      self@A.f(1, 2);
      not false;
      self;
    }
  };
};