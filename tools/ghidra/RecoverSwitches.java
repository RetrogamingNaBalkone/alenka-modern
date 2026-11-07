// @category Alenka
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import ghidra.program.model.pcode.JumpTable;
import java.util.ArrayList;

public class RecoverSwitches extends GhidraScript {
    public void run() throws Exception {
        Address jump = toAddr("1000:298b");
        ArrayList<Address> destinations = new ArrayList<>();
        for (int i=0; i<18; i++) {
            int key = getShort(toAddr("1000:4268").add(2*i)) & 0xffff;
            int offset = getShort(toAddr("1000:428c").add(2*i)) & 0xffff;
            Address target = toAddr(String.format("1000:%04x", offset));
            if (!destinations.contains(target)) destinations.add(target);
            println(String.format("scan %02x -> %s", key, target));
            disassemble(target);
            createLabel(target, String.format("key_%02x",key), true);
            currentProgram.getReferenceManager().addMemoryReference(jump, target, RefType.COMPUTED_JUMP, SourceType.USER_DEFINED, 0);
        }
        // Turbo C's sparse-switch helper isn't recognised by automatic analysis.
        Function main = getFunctionAt(toAddr("1000:25f6"));
        main.setBody(new AddressSet(toAddr("1000:25f6"), toAddr("1000:4267")));
        new JumpTable(jump, destinations, true, 0).writeOverride(main);
        ArrayList<Address> statTargets = new ArrayList<>();
        for (int i=0; i<16; i++) {
            int key = getShort(toAddr("1000:219f").add(2*i)) & 0xffff;
            int offset = getShort(toAddr("1000:21bf").add(2*i)) & 0xffff;
            Address target = toAddr(String.format("1000:%04x",offset));
            println(String.format("statistics scan %02x -> %s",key,target));
            disassemble(target);
            if (!statTargets.contains(target)) statTargets.add(target);
        }
        Function stats = getFunctionAt(toAddr("1000:1ecb"));
        stats.setBody(new AddressSet(toAddr("1000:1ecb"),toAddr("1000:219e")));
        new JumpTable(toAddr("1000:1faa"),statTargets,true,0).writeOverride(stats);
        // The routine immediately before menu rendering was missed by startup flow.
        // CS:01ff is the runtime DS word, not code. Startup's DOS exit call
        // otherwise falls through it and consumes the first byte of 0203.
        Function exit = getFunctionAt(toAddr("1000:0115"));
        if (exit != null) exit.setNoReturn(true);
        Function containing = getFunctionContaining(toAddr("1000:0203"));
        if (containing != null && !containing.getEntryPoint().equals(toAddr("1000:0203"))) {
            AddressSet remaining = new AddressSet(containing.getBody());
            remaining.delete(toAddr("1000:0203"),toAddr("1000:0341"));
            containing.setBody(remaining);
        }
        clearListing(toAddr("1000:01ff"),toAddr("1000:0205"));
        createWord(toAddr("1000:01ff"));
        disassemble(toAddr("1000:0203"));
        if (getFunctionAt(toAddr("1000:0203")) == null) createFunction(toAddr("1000:0203"), "record_typo");
        disassemble(toAddr("1000:21df"));
        if (getFunctionAt(toAddr("1000:21df")) == null) createFunction(toAddr("1000:21df"), "next_lesson");
        for (String address : new String[]{"1000:618f","1000:61c8","1000:620d","1000:62c8"}) {
            Address a = toAddr(address);
            disassemble(a);
            if (getFunctionAt(a)==null) createFunction(a,null);
        }
        analyzeChanges(currentProgram);
    }
}
