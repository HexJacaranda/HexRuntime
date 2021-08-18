import org.antlr.v4.runtime.CharStreams
import org.antlr.v4.runtime.CommonTokenStream

fun main() {
    val lexer = AssemblierLexer(CharStreams.fromFileName("C:\\Assembly\\assembly.il"))
    val parser = Assemblier(CommonTokenStream(lexer))
    val root = parser.start()
    val ir = AssemblyBuilder().build(root)
}