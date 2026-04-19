class A inherits IO {
  x : Int <- 0;
  y : Int <- 1;
  z : String <- "start";
  flag : Bool <- false;
  self_type_holder : SELF_TYPE;

  classx : Int <- 10;
  ifx : Int <- 20;
  thenx : Int <- 30;
  elsex : Int <- 40;
  fix : Int <- 50;
  inx : Int <- 60;
  inheritsx : Int <- 70;
  isvoidx : Int <- 80;
  letx : Int <- 90;
  loopx : Int <- 100;
  poolx : Int <- 110;
  casey : Int <- 120;
  esacx : Int <- 130;
  newx : Int <- 140;
  notx : Int <- 150;
  truex : Bool <- false;
  falsex : Bool <- true;

  f(a : Int, b : Int, c : String) : Object {
    {
      x <- a + b;
      y <- a * b / 2 - 3;
      z <- c.concat("::").concat("f");
      if a < b then
        out_string("a<b\n")
      else
        out_string("a>=b\n")
      fi;
      if a <= b then out_string("<=\n") else out_string(">\n") fi;
      if a = b then out_string("=\n") else out_string("!=\n") fi;
      not false;
      ~a;
      self;
    }
  };

  g() : SELF_TYPE {
    {
      out_string("g()\n");
      self;
    }
  };

  h() : Object {
    let i : Int <- 0,
        j : Int <- 5,
        s : String <- "abc\n\t\b\f\"\\",
        t : String <- "continued \
line",
        u : Bool <- true
    in {
      while i < j loop {
        out_int(i);
        out_string(":");
        out_string(s);
        out_string("\n");
        i <- i + 1;
      } pool;

      case i of
        aa : Int => aa + 1;
        bb : Object => 0;
      esac;

      self@A.g();
      self.f(i, j, s);
    }
  };
};

class B inherits A {
  value : Int <- 999;

  f(a : Int, b : Int, c : String) : Object {
    {
      out_string("B.f override\n");
      {
        a + b;
        a - b;
        a * b;
        a / b;
        a < b;
        a <= b;
        a = b;
      };
      self;
    }
  };
};

class Main inherits IO {

  main() : Object {
    {
      -- comentário de linha com lixo léxico que deve ser ignorado:
      -- @ # $ ? ` [ ] > ' " <- <= => class IF fi

      (*
        comentário nível 1
        class Fake inherits Nope {
          x : Int <- 123;
          y : String <- "isso não deve virar token";
        };

        (*
          comentário nível 2
          if then else fi while loop pool let in case of esac new isvoid not
          <= <- => ~ @ . , ; : { } ( )
          "string fake"
          -- comentário fake dentro de comentário de bloco
          (*
            comentário nível 3
            abcDEF123
            (* nível 4 *)
          *)
        *)

        fim do nível 1
      *)

      let obj : A <- new B,
          count : Int <- 3,
          msg : String <- "HELLO",
          empty : String <- "",
          truth : Bool <- true,
          lie : Bool <- false
      in {
        obj.g();
        obj.f(1, 2, msg);
        obj.h();

        while 0 < count loop {
          out_string("loop:\n");
          out_int(count);
          out_string("\n");
          count <- count - 1;
        } pool;

        case count of
          x : Int => out_int(x);
          y : Object => out_int(0);
        esac;

        if isvoid empty then
          out_string("void\n")
        else
          out_string("not void\n")
        fi;

        if truth then out_string("true\n") else out_string("false\n") fi;
        if lie then out_string("bad\n") else out_string("good\n") fi;

        {
          classx;
          ifx;
          thenx;
          elsex;
          fix;
          inx;
          inheritsx;
          isvoidx;
          letx;
          loopx;
          poolx;
          casey;
          esacx;
          newx;
          notx;
          truex;
          falsex;
        };

        out_string("keywords in weird case:\n");
        cLaSs;
        iFx;
        tHeNx;
        eLsEx;
        fIx;
      };

      0;
    }
  };
};

class KeywordCaseTrap {
  ClassVar : Int <- 1;
  IfVar : Int <- 2;
  InheritsVar : Int <- 3;
  IsvoidVar : Int <- 4;
  LetVar : Int <- 5;
  LoopVar : Int <- 6;
  PoolVar : Int <- 7;
  CaseVar : Int <- 8;
  EsacVar : Int <- 9;
  NewVar : Int <- 10;
  NotVar : Int <- 11;
  TrueVar : Bool <- false;
  FalseVar : Bool <- true;

  main() : Object {
    {
      ClassVar;
      IfVar;
      InheritsVar;
      IsvoidVar;
      LetVar;
      LoopVar;
      PoolVar;
      CaseVar;
      EsacVar;
      NewVar;
      NotVar;
      TrueVar;
      FalseVar;
    }
  };
};

(*
  A partir daqui começam erros léxicos intencionais.
  Seu lexer idealmente deve reportar vários deles.
*)

class LexicalHell {
  ok : Int <- 0;

  broken() : Object {
    {
      @;
      #;
      $;
      ?;
      `;
      [;
      ];
      >;
      'a';
      'b';
      '---';
      *);
      "unterminated string starts here
      ok <- 1;
    }
  };
};

(*
comentário aberto propositalmente no final