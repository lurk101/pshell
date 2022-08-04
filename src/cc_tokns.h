// clang-format off
    Func = 128,
    Syscall, Main, Glo, Par, Loc, Keyword, Id, Load, Enter, Num, NumF, Enum, Char, Int,
    Float, Struct, Union, Sizeof, Return, Goto, Break, Continue, If, DoWhile, While, For, Switch,
    Case, Default, Else, Label,
    Assign,   // operator =, keep Assign as highest priority operator
    OrAssign, // |=, ^=, &=, <<=, >>=
    XorAssign,
    AndAssign,
    ShlAssign,
    ShrAssign,
    AddAssign, // +=, -=, *=, /=, %=
    SubAssign,
    MulAssign,
    DivAssign,
    ModAssign,
    Cond, // operator: ?
    Lor,  // operator: ||, &&, |, ^, &
    Lan,
    Or,
    Xor,
    And,
    Eq, // operator: ==, !=, >=, <, >, <=
    Ne,
    Ge,
    Lt,
    Gt,
    Le,
    Shl, // operator: <<, >>, +, -, *, /, %
    Shr,
    Add,
    Sub,
    Mul,
    Div,
    Mod,
    AddF, // float type operators (hidden)
    SubF,
    MulF,
    DivF,
    EqF,
    NeF,
    GeF,
    LtF,
    GtF,
    LeF,
    CastF,
    Inc, // operator: ++, --, ., ->, [
    Dec,
    Dot,
    Arrow,
    Bracket
    // clang-format on
