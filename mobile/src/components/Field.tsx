import { forwardRef, useState } from 'react';
import { TextInput, View, type TextInputProps } from 'react-native';

import { fonts } from '@/theme/tokens';
import { makeStyles, useTheme } from '@/theme/useTheme';

import { T } from './T';

/**
 * .field — label 12px / 500 / 0.11em in --dim; transparent input with a 1px
 * --line border that turns --fg on focus, 13×14 padding.
 * `readOnly` shows the value in --dim (input[readonly]); an error adds an
 * --accent border.
 */
type Props = TextInputProps & {
  label: string;
  error?: boolean;
};

export const Field = forwardRef<TextInput, Props>(function Field(
  { label, error, multiline, onFocus, onBlur, style, readOnly, ...rest },
  ref,
) {
  const { c, name } = useTheme();
  const s = useStyles();
  const [focused, setFocused] = useState(false);

  return (
    <View style={s.field}>
      <T variant="label" size={12} tracking={0.11} color="dim" style={s.label}>
        {label}
      </T>
      <TextInput
        ref={ref}
        placeholderTextColor={c.dim2}
        selectionColor={c.accent}
        cursorColor={c.fg}
        keyboardAppearance={name === 'inverted' ? 'dark' : 'light'}
        multiline={multiline}
        readOnly={readOnly}
        maxFontSizeMultiplier={1.3}
        onFocus={(e) => {
          setFocused(true);
          onFocus?.(e);
        }}
        onBlur={(e) => {
          setFocused(false);
          onBlur?.(e);
        }}
        accessibilityLabel={label}
        style={[
          s.input,
          multiline && s.multiline,
          focused && !readOnly && s.focused,
          !!error && s.error,
          readOnly && s.readOnly,
          style,
        ]}
        {...rest}
      />
    </View>
  );
});

const useStyles = makeStyles((c) => ({
  field: { marginBottom: 18 },
  label: { marginBottom: 8 },
  input: {
    backgroundColor: 'transparent',
    borderWidth: 1,
    borderColor: c.line,
    color: c.fg,
    paddingVertical: 13,
    paddingHorizontal: 14,
    fontFamily: fonts.regular,
    // 16px keeps iOS from zooming the page into a focused field (the website uses 14px).
    fontSize: 16,
    minHeight: 48,
    // .field input:focus { outline: none } — the border carries focus instead.
    outlineWidth: 0,
  },
  multiline: { minHeight: 100, textAlignVertical: 'top' },
  focused: { borderColor: c.fg },
  error: { borderColor: c.accent },
  readOnly: { color: c.dim },
}));
