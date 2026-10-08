package clipkey;

import java.awt.BorderLayout;
import java.awt.Color;
import java.awt.Dimension;
import java.awt.FlowLayout;
import java.awt.Font;
import java.awt.GridBagConstraints;
import java.awt.GridBagLayout;
import java.awt.Insets;
import java.awt.Toolkit;
import java.awt.datatransfer.DataFlavor;
import java.awt.event.WindowAdapter;
import java.awt.event.WindowEvent;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.function.Consumer;
import javax.swing.BorderFactory;
import javax.swing.BoxLayout;
import javax.swing.JButton;
import javax.swing.JCheckBox;
import javax.swing.JDialog;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JOptionPane;
import javax.swing.JPanel;
import javax.swing.JPasswordField;
import javax.swing.JProgressBar;
import javax.swing.JScrollPane;
import javax.swing.JSpinner;
import javax.swing.JTextArea;
import javax.swing.JTextField;
import javax.swing.SpinnerNumberModel;
import javax.swing.SwingUtilities;
import javax.swing.Timer;
import javax.swing.UIManager;
import javax.swing.event.DocumentEvent;
import javax.swing.event.DocumentListener;

/** 창 모드. 인자가 있으면 CLI(ClipKeySender)로 넘긴다. */
public final class ClipKeyApp {
    private static final int POLL_MS = 500;

    private final JFrame frame = new JFrame("ClipKey Bridge");
    private final JTextArea preview = new JTextArea();
    private final JLabel info = new JLabel(" ");
    private final JLabel device = new JLabel(" ");
    private final JCheckBox enter = new JCheckBox("마지막 Enter 추가", Settings.appendEnter());
    private final JCheckBox auto = new JCheckBox("버튼 없이 2초 후 자동 입력", Settings.autoStart());
    private final JSpinner delay = new JSpinner(new SpinnerNumberModel(Settings.delayMs(), 10, 100, 10));
    private final JProgressBar progress = new JProgressBar(0, 1);
    private final JButton reload = new JButton("클립보드 다시 읽기");
    private final JButton send = new JButton("보내기");
    private final JButton cancel = new JButton("취소");
    private final Timer poller = new Timer(POLL_MS, e -> poll());
    private final ExecutorService net = Executors.newSingleThreadExecutor(r -> {
        Thread t = new Thread(r, "clipkey-net");
        t.setDaemon(true);
        return t;
    });

    private String text = "";
    private String activeJob = null;
    private boolean polling = false;
    private boolean loading = false;  // 프로그램이 미리보기를 채우는 중 (편집으로 세지 않음)
    private boolean edited = false;   // 클립보드를 읽은 뒤 사용자가 고쳤는가

    public static void main(String[] args) {
        if (args.length > 0) {
            ClipKeySender.main(args);
            return;
        }
        System.setProperty("apple.awt.application.name", "ClipKey Bridge");
        try { UIManager.setLookAndFeel(UIManager.getSystemLookAndFeelClassName()); } catch (Exception ignored) {}
        SwingUtilities.invokeLater(() -> new ClipKeyApp().show());
    }

    private void show() {
        preview.setFont(new Font(Font.MONOSPACED, Font.PLAIN, 13));
        preview.setTabSize(4);
        preview.getDocument().addDocumentListener(new DocumentListener() {
            @Override public void insertUpdate(DocumentEvent e) { onEdit(); }
            @Override public void removeUpdate(DocumentEvent e) { onEdit(); }
            @Override public void changedUpdate(DocumentEvent e) { onEdit(); }
        });
        progress.setStringPainted(true);
        progress.setString("");
        cancel.setEnabled(false);

        JPanel top = new JPanel(new BorderLayout());
        top.add(device, BorderLayout.CENTER);
        JButton settings = new JButton("설정…");
        settings.addActionListener(e -> showSettings());
        top.add(settings, BorderLayout.EAST);

        JPanel options = new JPanel(new FlowLayout(FlowLayout.LEFT, 8, 0));
        options.add(enter);
        options.add(auto);
        options.add(new JLabel("글자 간격(ms)"));
        options.add(delay);

        JPanel buttons = new JPanel(new FlowLayout(FlowLayout.RIGHT, 8, 0));
        buttons.add(reload);
        buttons.add(cancel);
        buttons.add(send);

        JPanel bottom = new JPanel();
        bottom.setLayout(new BoxLayout(bottom, BoxLayout.Y_AXIS));
        for (var c : new java.awt.Component[]{info, options, progress, buttons}) {
            if (c instanceof JPanel p) p.setAlignmentX(0f);
            bottom.add(c);
            bottom.add(javax.swing.Box.createVerticalStrut(6));
        }

        JPanel root = new JPanel(new BorderLayout(0, 8));
        root.setBorder(BorderFactory.createEmptyBorder(10, 12, 10, 12));
        root.add(top, BorderLayout.NORTH);
        root.add(new JScrollPane(preview), BorderLayout.CENTER);
        root.add(bottom, BorderLayout.SOUTH);

        reload.addActionListener(e -> readClipboard());
        send.addActionListener(e -> submit());
        cancel.addActionListener(e -> cancelJob());
        frame.addWindowListener(new WindowAdapter() {
            // 편집 중인 내용은 창을 오가도 덮어쓰지 않는다. 되돌리려면 '클립보드 다시 읽기'.
            @Override public void windowActivated(WindowEvent e) { if (activeJob == null && !edited) readClipboard(); }
        });

        frame.setContentPane(root);
        frame.setDefaultCloseOperation(JFrame.EXIT_ON_CLOSE);
        frame.setMinimumSize(new Dimension(520, 420));
        frame.setLocationByPlatform(true);
        frame.pack();
        frame.setVisible(true);
        refreshDevice();
        if (!Settings.configured()) showSettings();
    }

    // ---------- 클립보드 ----------

    private void readClipboard() {
        try {
            Object data = Toolkit.getDefaultToolkit().getSystemClipboard().getData(DataFlavor.stringFlavor);
            setText(ClipKeyText.normalize(String.valueOf(data)));
        } catch (Exception e) {
            setText("");
            info.setText("클립보드에 텍스트가 없습니다");
        }
    }

    private void setText(String t) {
        loading = true;
        preview.setText(t);
        preview.setCaretPosition(0);
        loading = false;
        edited = false;
        updateText(t);
    }

    private void onEdit() {
        if (loading) return;
        edited = true;
        updateText(ClipKeyText.normalize(preview.getText()));
    }

    private void updateText(String t) {
        text = t;
        String problem = ClipKeyText.problem(t);
        boolean ok = problem == null;
        info.setForeground(ok ? UIManager.getColor("Label.foreground") : new Color(0xB00020));
        info.setText((ok ? t.length() + "자, " + (t.split("\n", -1).length) + "줄" : problem) + (edited ? "  (편집됨)" : ""));
        send.setEnabled(ok && activeJob == null);
    }

    // ---------- 전송 / 진행 ----------

    private void submit() {
        ClipKeyClient client = client();
        if (client == null) return;
        boolean appendEnter = enter.isSelected(), autoStart = auto.isSelected();
        int delayMs = (Integer) delay.getValue();
        Settings.save(Settings.url(), Settings.token(), delayMs, appendEnter, autoStart);
        setBusy(true);
        progress.setValue(0);
        progress.setString("등록 중…");
        run(() -> client.submit(text, appendEnter, autoStart, delayMs), job -> {
            activeJob = job.requestId();
            showJob(job);
            poller.start();
        }, err -> {
            setBusy(false);
            progress.setString("");
            info.setText("전송 실패: " + err);
        });
    }

    private void poll() {
        if (polling || activeJob == null) return;
        ClipKeyClient client = client();
        if (client == null) return;
        polling = true;
        String id = activeJob;
        run(() -> client.job(id), job -> {
            polling = false;
            showJob(job);
            if (!job.active()) finishJob();
        }, err -> {
            polling = false;
            progress.setString("상태 확인 실패: " + err);
            if (err.contains("HTTP 404")) finishJob();  // 보드 재부팅 등으로 작업이 사라짐. 결과는 알 수 없음
        });
    }

    private void cancelJob() {
        ClipKeyClient client = client();
        if (client == null || activeJob == null) return;
        String id = activeJob;
        run(() -> client.cancel(id), this::showJob, err -> progress.setString("취소 실패: " + err));
    }

    private void showJob(ClipKeyClient.Job job) {
        progress.setMaximum(Math.max(1, job.total()));
        progress.setValue(job.typed());
        progress.setString(switch (job.state()) {
            case "WAITING" -> auto.isSelected() ? "2초 뒤 자동 입력 — 멈추려면 취소 또는 보드 BOOT"
                                                : "보드의 BOOT 버튼을 누르면 입력을 시작합니다 (60초 내)";
            case "TYPING" -> "입력 중 " + job.typed() + " / " + job.total();
            case "COMPLETED" -> "완료 " + job.typed() + " / " + job.total();
            case "CANCELLED" -> "취소됨 (" + job.error() + ") " + job.typed() + " / " + job.total();
            case "FAILED" -> "실패 (" + job.error() + ") " + job.typed() + " / " + job.total();
            default -> job.state();
        });
    }

    private void finishJob() {
        poller.stop();
        activeJob = null;
        setBusy(false);
        refreshDevice();
    }

    private void setBusy(boolean busy) {
        send.setEnabled(!busy && ClipKeyText.problem(text) == null);
        cancel.setEnabled(busy);
        reload.setEnabled(!busy);
        preview.setEditable(!busy);
        enter.setEnabled(!busy);
        auto.setEnabled(!busy);
        delay.setEnabled(!busy);
    }

    // ---------- 장치 / 설정 ----------

    private ClipKeyClient client() {
        try {
            return new ClipKeyClient(Settings.url(), Settings.token());
        } catch (IllegalArgumentException e) {
            info.setText("설정 확인: " + e.getMessage());
            return null;
        }
    }

    private void refreshDevice() {
        device.setText(Settings.url() + " — 확인 중…");
        ClipKeyClient client = client();
        if (client == null) { device.setText(Settings.url() + " — 설정 필요"); return; }
        run(client::status,
                s -> device.setText(Settings.url() + " — " + s.firmwareVersion() + ", USB "
                        + (s.usbReady() ? "연결됨" : "미연결") + ", " + s.state()),
                err -> device.setText(Settings.url() + " — 연결 안 됨 (" + err + ")"));
    }

    private void showSettings() {
        JDialog dialog = new JDialog(frame, "설정", true);
        JTextField url = new JTextField(Settings.url(), 28);
        JPasswordField token = new JPasswordField(Settings.token(), 28);
        JLabel result = new JLabel(" ");
        JButton test = new JButton("연결 테스트");
        JButton save = new JButton("저장");

        test.addActionListener(e -> {
            try {
                ClipKeyClient c = new ClipKeyClient(url.getText(), new String(token.getPassword()));
                result.setText("확인 중…");
                run(c::status, s -> result.setText("OK: " + s.firmwareVersion() + ", USB " + (s.usbReady() ? "연결됨" : "미연결")),
                        err -> result.setText("실패: " + err));
            } catch (IllegalArgumentException ex) {
                result.setText(ex.getMessage());
            }
        });
        save.addActionListener(e -> {
            try {
                ClipKeyClient.parseBase(url.getText());
                Settings.save(url.getText(), new String(token.getPassword()), (Integer) delay.getValue(),
                        enter.isSelected(), auto.isSelected());
                dialog.dispose();
                refreshDevice();
            } catch (IllegalArgumentException ex) {
                result.setText(ex.getMessage());
            }
        });

        JPanel form = new JPanel(new GridBagLayout());
        form.setBorder(BorderFactory.createEmptyBorder(12, 12, 12, 12));
        GridBagConstraints g = new GridBagConstraints();
        g.insets = new Insets(4, 4, 4, 4);
        g.anchor = GridBagConstraints.WEST;
        g.gridy = 0; g.gridx = 0; form.add(new JLabel("장치 주소"), g);
        g.gridx = 1; g.fill = GridBagConstraints.HORIZONTAL; g.weightx = 1; form.add(url, g);
        g.gridy = 1; g.gridx = 0; g.fill = GridBagConstraints.NONE; g.weightx = 0; form.add(new JLabel("토큰"), g);
        g.gridx = 1; g.fill = GridBagConstraints.HORIZONTAL; g.weightx = 1; form.add(token, g);
        g.gridy = 2; g.gridx = 0; g.gridwidth = 2; form.add(new JLabel("<html><small>주소 예: http://clipkey.local — 토큰은 보드 secrets.h 의 CLIPKEY_TOKEN 과 같은 값</small></html>"), g);
        g.gridy = 3; form.add(result, g);
        JPanel actions = new JPanel(new FlowLayout(FlowLayout.RIGHT));
        actions.add(test);
        actions.add(save);
        g.gridy = 4; g.anchor = GridBagConstraints.EAST; form.add(actions, g);

        dialog.setContentPane(form);
        dialog.pack();
        dialog.setLocationRelativeTo(frame);
        dialog.setVisible(true);
    }

    // ---------- 네트워크 호출을 EDT 밖에서 ----------

    private interface Call<T> { T get() throws Exception; }

    private <T> void run(Call<T> call, Consumer<T> onOk, Consumer<String> onErr) {
        net.submit(() -> {
            try {
                T v = call.get();
                SwingUtilities.invokeLater(() -> onOk.accept(v));
            } catch (Exception e) {
                String msg = e instanceof ClipKeyClient.ApiException ? e.getMessage()
                        : e.getClass().getSimpleName() + (e.getMessage() == null ? "" : ": " + e.getMessage());
                SwingUtilities.invokeLater(() -> onErr.accept(msg));
            }
        });
    }
}
