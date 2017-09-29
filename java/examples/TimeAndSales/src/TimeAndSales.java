import io.github.jeog.tosdatabridge.*;

import javax.swing.*;
import javax.swing.table.DefaultTableCellRenderer;
import javax.swing.table.DefaultTableModel;
import java.awt.*;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.util.ArrayList;
import java.util.Set;
import java.util.List;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class TimeAndSales {
    public static final boolean IS_32_BIT_ARCH = System.getProperty("os.arch").equals("x86");
    public static final String DLL_BASE_REGEX = "tos-databridge-\\d{1,}\\.\\d{1,}-x";
    public static final String DLL_DIRECTORY_PATH;
    static{
        String ourPath = TimeAndSales.class.getProtectionDomain().getCodeSource().getLocation().getPath();
        int indx = ourPath.indexOf("TOSDataBridge/java/examples");
        if( indx != -1 ) {
            DLL_DIRECTORY_PATH = ourPath.substring(0, indx) + "TOSDataBridge/bin/Release/" +
                    (IS_32_BIT_ARCH ? "Win32/" : "x64/");
        }else{
            DLL_DIRECTORY_PATH  = ourPath;
        }
    }

    public static final int DATA_BLOCK_SIZE = 1000;
    public static final long LATENCY = 100;
    public static final int TIMESALE_TABLE_MAX_ROWS = 300;
    public static final int TIMESALE_TABLE_WIDTH = 350;
    public static final int TIMESALE_TABLE_HEIGHT = 800;

    private static JFrame myFrame;
    private static JPanel myPanel;
    private static JToolBar myToolBar;
    private static JTextField mySymbolEntryTextField;
    private static JButton mySymbolEntryButton;
    private static JScrollPane myScrollPane;
    private static JTable myTable;
    private static JFileChooser myFileChooser;
    private static DataBlockWithDateTime myDataBlock;
    private static DataRetrievalThread myDataRetrievalThread;
    private static List<Color> myRowColors = new ArrayList<>();

    private static class DataRetrievalThread extends Thread{
        private String symbol;
        private long volume;
        private double previousPrice;

        public
        DataRetrievalThread(String symbol){
            this.symbol = symbol;
        }

        @Override
        public void
        run(){
            while( !interrupted() ){
                try {
                    final DateTime.DateTimePair<Double> last = myDataBlock.getDoubleWithDateTime(symbol, Topic.LAST);
                    Long volume = myDataBlock.getLong(symbol, Topic.VOLUME);
                    if( last != null && volume != null ) {
                        final long volumeDiff = volume - this.volume;
                        if (this.volume != 0 && volumeDiff > 0) {
                            final Color rowColor;
                            if( this.previousPrice == 0 || this.previousPrice == last.first ){
                                rowColor = Color.WHITE;
                            }else {
                                rowColor = (last.first  > this.previousPrice) ? Color.GREEN : Color.RED;
                            }
                            this.previousPrice = last.first;
                            SwingUtilities.invokeLater(new Runnable() {
                                @Override
                                public void run() {
                                    DefaultTableModel tableModel = (DefaultTableModel) myTable.getModel();
                                    tableModel.insertRow(0, new Object[]{ last.second.toString(),
                                            String.valueOf(last.first), String.valueOf(volumeDiff) });
                                    myRowColors.add(0, rowColor);
                                    int rowCount = tableModel.getRowCount();
                                    if( rowCount > TIMESALE_TABLE_MAX_ROWS ){
                                        tableModel.removeRow(rowCount - 1);
                                        myRowColors.remove(rowCount - 1);
                                        rowCount--;
                                    }
                                    myFrame.repaint();
                                }
                            });
                        }
                        this.volume = volume;
                    }
                }catch( TOSDataBridge.TOSDataBridgeException e ){
                    displayMessage("Error", "Error retrieving data from block: " + e.getMessage(),
                            JOptionPane.ERROR_MESSAGE);
                    this.interrupt();
                    break;
                }
                try{
                    Thread.sleep(LATENCY);
                }catch( InterruptedException e ){
                    this.interrupt();
                    break;
                }
            }
            SwingUtilities.invokeLater(new Runnable() {
                @Override
                public void run() {
                    DefaultTableModel tableModel = (DefaultTableModel) myTable.getModel();
                    tableModel.setRowCount(0);
                    myFrame.repaint();
                }
            });
        }
    }


    public static void
    main(String[] args){
        SwingUtilities.invokeLater(new Runnable() {
            @Override
            public void run() {
                setupGUI();
                myFileChooser = new JFileChooser(DLL_DIRECTORY_PATH);
                myFileChooser.setDialogTitle("Load TOSDataBridge DLL");
                while( true ){
                    int response = myFileChooser.showOpenDialog(null);
                    if (response != JFileChooser.APPROVE_OPTION) {
                        System.exit(0);
                    }
                    String dllPath = myFileChooser.getSelectedFile().getPath();
                    Pattern dllPattern = Pattern.compile(DLL_BASE_REGEX + "(64|86)\\.dll");
                    Matcher dllMatcher = dllPattern.matcher(dllPath);
                    if( !dllMatcher.find() ){
                        displayMessage("Error", "File doesn't appear to be a valid TOSDataBridge DLL",
                                JOptionPane.ERROR_MESSAGE);
                        continue;
                    }
                    if( !dllMatcher.group(1).equals(IS_32_BIT_ARCH ? "86" : "64") ){
                        displayMessage("Error", "JVM is " + (IS_32_BIT_ARCH ? "32" : "64") + " bit. '" +
                                        dllMatcher.group() + "' appears to be the wrong build for this JVM.",
                                JOptionPane.ERROR_MESSAGE);
                        continue;
                    }
                    if( setupTOSDataBridge(dllPath) ){
                        break;
                    }
                }
                Runtime.getRuntime().addShutdownHook( new Thread(){
                    @Override
                    public void run(){
                        if( myDataRetrievalThread != null ){
                            myDataRetrievalThread.interrupt();
                            try {
                                myDataRetrievalThread.join();
                            }catch( InterruptedException e ){
                                e.printStackTrace();
                                Thread.currentThread().interrupt();
                            }
                        }
                        if( myDataBlock != null ){
                            try {
                                myDataBlock.close();
                            }catch( TOSDataBridge.TOSDataBridgeException e ){
                                e.printStackTrace();
                            }
                        }
                    }
                });
            }
        });
    }

    private static boolean
    setupTOSDataBridge(String dllPath){
        try {
            TOSDataBridge.init(dllPath);
            switch( TOSDataBridge.connectionState() ){
                case TOSDataBridge.CONN_NONE:
                    displayMessage("Error", "Failed to Connect to Engine (CONN_NONE)",
                            JOptionPane.ERROR_MESSAGE);
                    return false;
                case TOSDataBridge.CONN_ENGINE:
                    displayMessage("Error", "Failed to Connect to TOS (CONN_ENGINE)",
                            JOptionPane.ERROR_MESSAGE);
                    return false;
                case TOSDataBridge.CONN_ENGINE_TOS:
                    displayMessage("Success", "Connected to Engine and TOS (CONN_ENGINE_TOS)",
                            JOptionPane.INFORMATION_MESSAGE);
            }
            myDataBlock = new DataBlockWithDateTime(DATA_BLOCK_SIZE);
            myDataBlock.addTopic(Topic.LAST);
            myDataBlock.addTopic(Topic.VOLUME);
            return true;
        }catch( TOSDataBridge.TOSDataBridgeException e ){
            displayMessage("Error", "TOSDataBridge Exception: " + e.getMessage(), JOptionPane.ERROR_MESSAGE);
            return false;
        }
    }

    private static void
    setupGUI(){
        myFrame = new JFrame(TimeAndSales.class.getSimpleName());
        myFrame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        myPanel = new JPanel(new BorderLayout());
        myToolBar = new JToolBar();
        myTable = new JTable( new MyTableModel() );
        myTable.setDefaultRenderer(Object.class, new MyTableCellRenderer());
        myTable.getColumnModel().getColumn(0).setPreferredWidth(180);
        myScrollPane = new JScrollPane(myTable, JScrollPane.VERTICAL_SCROLLBAR_AS_NEEDED,
                JScrollPane.HORIZONTAL_SCROLLBAR_AS_NEEDED);
        myToolBar.setComponentOrientation(ComponentOrientation.LEFT_TO_RIGHT);
        myToolBar.setMargin(new Insets(2,2,2,2));
        mySymbolEntryButton = new JButton("Enter");
        mySymbolEntryButton.addActionListener( new EntryButtonListener() );
        mySymbolEntryTextField = new JTextField("",6);
        mySymbolEntryTextField.setMaximumSize(mySymbolEntryTextField.getPreferredSize());
        myPanel.setPreferredSize(new Dimension(TIMESALE_TABLE_WIDTH, TIMESALE_TABLE_HEIGHT));
        mySymbolEntryButton.setPreferredSize(new Dimension(40,30));
        myToolBar.setPreferredSize(new Dimension(TIMESALE_TABLE_WIDTH,30));
        myToolBar.add(mySymbolEntryTextField);
        myToolBar.add(mySymbolEntryButton);
        myPanel.add(myToolBar, BorderLayout.PAGE_START);
        myPanel.add(myScrollPane, BorderLayout.CENTER);
        myToolBar.setFloatable(false);
        myFrame.setContentPane(myPanel);
        myFrame.getRootPane().setDefaultButton(mySymbolEntryButton);
        myFrame.pack();
        myFrame.setLocationRelativeTo(null);
        myFrame.setVisible(true);
    }

    private static class MyTableModel extends DefaultTableModel{
        public
        MyTableModel(){
            super( new String[]{"Time", "Price", "Volume"}, 0);
        }
        @Override
        public boolean
        isCellEditable(int row, int column){
            return false;
        }
    }

    private static class MyTableCellRenderer extends DefaultTableCellRenderer{
        @Override
        public Component
        getTableCellRendererComponent(JTable table, Object value, boolean isSelected, boolean hasFocus,
                                      int row, int column){
            final Component c = super.getTableCellRendererComponent(table, value, isSelected, hasFocus, row, column);
            c.setBackground( myRowColors.get(row) );
            return c;
        }
    }

    private static class
    EntryButtonListener implements ActionListener {
        @Override
        public void
        actionPerformed(ActionEvent actionEvent){
            String symbol = mySymbolEntryTextField.getText();
            try {
                if( myDataRetrievalThread != null ){
                    myDataRetrievalThread.interrupt();
                    myDataRetrievalThread.join();

                }
                Set<String> oldSymbols = myDataBlock.getItems();
                myDataBlock.addItem( symbol );
                for( String item : oldSymbols ){
                    myDataBlock.removeItem(item);
                }
                myDataRetrievalThread = new DataRetrievalThread(symbol);
                myDataRetrievalThread.start();
            }catch( TOSDataBridge.TOSDataBridgeException e ){
                displayMessage("Error", "Error adding symbol '" + symbol + "':" + e.getMessage(),
                        JOptionPane.ERROR_MESSAGE);
            }catch( InterruptedException e ){
                Thread.currentThread().interrupt();
            }
        }
    }

    private static void
    displayMessage(final String title, final String message, final int messageType){
        SwingUtilities.invokeLater(
                new Runnable() {
                     @Override
                     public void run() {
                         JOptionPane.showMessageDialog(null, message, title, messageType);
                     }
                 }
        );
    }

}
