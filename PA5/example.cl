class A {
  x : Int <- 10;

  f() : Int {
    x + 1
  };

  name() : String {
    "A"
  };
};

class B inherits A {
  y : Int <- 20;

  f() : Int {
    x + y
  };

  name() : String {
    "B"
  };
};

class Maker inherits IO {
  make() : SELF_TYPE {
    new SELF_TYPE
  };

  who() : String {
    "Maker"
  };
};

class SpecialMaker inherits Maker {
  z : Int <- 99;

  who() : String {
    "SpecialMaker"
  };

  get_z() : Int {
    z
  };
};

class Main inherits IO {
  print_int_line(n : Int) : Object {
    {
      out_int(n);
      out_string("\n");
    }
  };

  print_str_line(s : String) : Object {
    {
      out_string(s);
      out_string("\n");
    }
  };

  main() : Object {
    {
      out_string("BEGIN\n");

      let a : A <- new A,
          b : B <- new B,
          aa : A <- new B
      in {
        out_string("dispatch:\n");

        -- A.f()
        print_int_line(a.f());

        -- B.f()
        print_int_line(b.f());

        -- Dynamic dispatch through static type A, runtime type B.
        print_int_line(aa.f());

        -- Static dispatch: force A.f() even though receiver is B.
        print_int_line(b@A.f());

        out_string("attrs/arithmetic:\n");
        print_int_line((1 + 2) * 3 - 4 / 2);

        out_string("comparison:\n");

        if 3 < 4 then
          out_string("lt-ok\n")
        else
          out_string("lt-bad\n")
        fi;

        if 4 <= 4 then
          out_string("le-ok\n")
        else
          out_string("le-bad\n")
        fi;

        if 5 = 5 then
          out_string("eq-int-ok\n")
        else
          out_string("eq-int-bad\n")
        fi;

        if "x" = "x" then
          out_string("eq-str-ok\n")
        else
          out_string("eq-str-bad\n")
        fi;

        out_string("loop:\n");

        let i : Int <- 0 in {
          while i < 3 loop {
            print_int_line(i);
            i <- i + 1;
          } pool;
        };

        out_string("case:\n");

        case aa of
          bb : B => out_string("case-B\n");
          ax : A => out_string("case-A\n");
          ox : Object => out_string("case-Object\n");
        esac;

        out_string("isvoid:\n");

        let v : A in
          if isvoid v then
            out_string("void-ok\n")
          else
            out_string("void-bad\n")
          fi;

        out_string("selftype:\n");

        let m : Maker <- new SpecialMaker in {
          out_string(m.make().who());
          out_string("\n");
        };

        out_string("END\n");
      };
    }
  };
};