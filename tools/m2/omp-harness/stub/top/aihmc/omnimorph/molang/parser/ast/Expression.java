package top.aihmc.omnimorph.molang.parser.ast;

/** Harness stub: an expression is its source text. */
public interface Expression {
    String text();

    record Text(String text) implements Expression {
    }
}
