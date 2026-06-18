class A {
	f() : Int {
		1
	};
};

class Main {
	main() : Int {
		case new A of
			x : A => x.f();
			y : Object => 0;
		esac
	};
};
