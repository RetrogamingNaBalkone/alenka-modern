// Headless Ghidra script: export decompilation and assembly for the DOS original.
// @category Alenka
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import java.io.*;

public class ExportAnalysis extends GhidraScript {
    public void run() throws Exception {
        File dir = new File(getScriptArgs()[0]);
        dir.mkdirs();
        DecompInterface decompiler = new DecompInterface();
        decompiler.openProgram(currentProgram);
        try (PrintWriter out = new PrintWriter(new File(dir, currentProgram.getName()+".c"), "UTF-8");
             PrintWriter asm = new PrintWriter(new File(dir, currentProgram.getName()+".asm"), "UTF-8")) {
            out.println("// Ghidra " + getGhidraVersion() + " " + currentProgram.getLanguageID());
            for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
                out.println("\n// ADDRESS " + f.getEntryPoint());
                DecompileResults result = decompiler.decompileFunction(f, 30, monitor);
                if (result.decompileCompleted()) out.println(result.getDecompiledFunction().getC());
                else out.println("// " + result.getErrorMessage());
            }
            for (Instruction i : currentProgram.getListing().getInstructions(true))
                asm.println(i.getAddress() + "  " + i);
        } finally { decompiler.dispose(); }
    }
}
