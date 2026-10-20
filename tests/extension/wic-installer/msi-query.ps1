# SPDX-License-Identifier: MIT
# Read a single field from the suite's shared read-only MSI connection.
function Read-MsiField($Database, [string]$Sql) {
    $view = $record = $null
    try {
        $view = Invoke-MsiMethod $Database 'OpenView' @($Sql)
        [void](Invoke-MsiMethod $view 'Execute')
        $record = Invoke-MsiMethod $view 'Fetch'
        if ($null -ne $record) {
            $record.GetType().InvokeMember('StringData', 'GetProperty', $null, $record, @(1))
        }
    } finally {
        Close-MsiObject $record
        Close-MsiObject $view
    }
}
